#!/usr/bin/env python3
"""Generate and verify the private Aris/ArisBootstrap runtime-pair binding."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import struct
import tempfile
from pathlib import Path


SCHEMA_FIELDS = {
    "arisSha256",
    "arisSize",
    "bootstrapSha256",
    "bootstrapSize",
    "buildId",
    "channel",
    "config",
    "packageKeyId",
    "publicKeyDigest",
    "schemaVersion",
    "sourceCommit",
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def signing_stable_pe_identity(path: Path) -> tuple[str, int]:
    """Hash PE bytes while excluding fields changed by Authenticode signing."""
    data = path.read_bytes()
    if len(data) < 0x40 or data[:2] != b"MZ":
        raise ValueError("Bootstrap must be a valid PE image")
    pe_offset = struct.unpack_from("<I", data, 0x3C)[0]
    if pe_offset > len(data) - 24 or data[pe_offset:pe_offset + 4] != b"PE\0\0":
        raise ValueError("Bootstrap PE header is invalid")
    optional_size = struct.unpack_from("<H", data, pe_offset + 20)[0]
    optional_offset = pe_offset + 24
    if optional_offset > len(data) - optional_size:
        raise ValueError("Bootstrap PE optional header is truncated")
    magic = struct.unpack_from("<H", data, optional_offset)[0]
    if magic == 0x10B:
        security_offset = optional_offset + 128
    elif magic == 0x20B:
        security_offset = optional_offset + 144
    else:
        raise ValueError("Bootstrap PE optional header has an unsupported format")
    checksum_offset = optional_offset + 64
    optional_end = optional_offset + optional_size
    if checksum_offset + 4 > optional_end or security_offset + 8 > optional_end:
        raise ValueError("Bootstrap PE optional header is incomplete")

    certificate_offset, certificate_size = struct.unpack_from(
        "<II", data, security_offset
    )
    if bool(certificate_offset) != bool(certificate_size):
        raise ValueError("Bootstrap PE certificate table is malformed")
    if certificate_offset:
        if (
            certificate_offset % 8 != 0
            or certificate_size < 8
            or certificate_offset < optional_end
            or certificate_offset + certificate_size != len(data)
        ):
            raise ValueError("Bootstrap PE certificate table is not a terminal overlay")
        image_size = certificate_offset
    else:
        image_size = len(data)
    if security_offset + 8 > image_size:
        raise ValueError("Bootstrap PE image is truncated before its data directories")

    digest = hashlib.sha256()
    digest.update(data[:checksum_offset])
    digest.update(data[checksum_offset + 4:security_offset])
    digest.update(data[security_offset + 8:image_size])
    return digest.hexdigest(), image_size


def contains_bytes(path: Path, needle: bytes) -> bool:
    overlap = max(0, len(needle) - 1)
    suffix = b""
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            window = suffix + chunk
            if needle in window:
                return True
            suffix = window[-overlap:] if overlap else b""
    return False


def atomic_write(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    handle, temporary = tempfile.mkstemp(prefix=path.name + ".", dir=path.parent)
    try:
        with os.fdopen(handle, "wb") as output:
            output.write(data)
            output.flush()
            os.fsync(output.fileno())
        os.replace(temporary, path)
    except BaseException:
        try:
            os.unlink(temporary)
        except FileNotFoundError:
            pass
        raise


def validate_metadata(args: argparse.Namespace) -> dict[str, object]:
    if not re.fullmatch(r"[0-9a-f]{40}", args.source_commit):
        raise ValueError("source commit must be exactly 40 lowercase hex characters")
    if not re.fullmatch(r"[A-Za-z0-9_.-]{1,128}", args.build_id):
        raise ValueError("build id contains unsupported characters")
    if args.channel not in {"qa", "testing", "stable"}:
        raise ValueError("channel is outside the closed runtime-pair set")
    if args.config not in {"Debug", "Release", "RelWithDebInfo", "MinSizeRel"}:
        raise ValueError("config is outside the closed runtime-pair set")
    if not re.fullmatch(r"[A-Za-z0-9_.-]{1,64}", args.key_id):
        raise ValueError("package key id is missing or malformed")
    if not re.fullmatch(r"[0-9a-f]{64}", args.public_key_hex or ""):
        raise ValueError("public key must be exactly 32 lowercase hex bytes")
    if set(args.public_key_hex) == {"0"}:
        raise ValueError("public key must be nonzero")
    return {
        "buildId": args.build_id,
        "channel": args.channel,
        "config": args.config,
        "packageKeyId": args.key_id,
        "publicKeyDigest": hashlib.sha256(bytes.fromhex(args.public_key_hex)).hexdigest(),
        "sourceCommit": args.source_commit,
    }


def require_file(path: Path, label: str) -> None:
    if not path.is_file() or path.is_symlink():
        raise ValueError(f"{label} must be a regular non-symlink file")


def bootstrap_header(args: argparse.Namespace) -> None:
    require_file(args.bootstrap, "Bootstrap")
    metadata = validate_metadata(args)
    digest, size = signing_stable_pe_identity(args.bootstrap)
    values = metadata
    lines = [
        "// Generated by tools/generate_runtime_pair.py. Do not edit.",
        "#pragma once",
        "",
        "#include <cstdint>",
        "#include <string_view>",
        "",
        "namespace aris::runtime_pair {",
    ]
    for key in (
        "sourceCommit", "buildId", "channel", "config", "packageKeyId",
        "publicKeyDigest",
    ):
        lines.append(f'inline constexpr std::string_view k{key[0].upper() + key[1:]} = "{values[key]}";')
    lines.extend([
        f'inline constexpr std::string_view kBootstrapImageSha256 = "{digest}";',
        f"inline constexpr std::uint64_t kBootstrapImageSize = {size}ULL;",
        "}",
        "",
    ])
    atomic_write(args.output, "\n".join(lines).encode("utf-8"))


def pair_document(args: argparse.Namespace) -> dict[str, object]:
    require_file(args.aris, "Aris")
    require_file(args.bootstrap, "Bootstrap")
    return {
        **validate_metadata(args),
        "arisSha256": sha256(args.aris),
        "arisSize": args.aris.stat().st_size,
        "bootstrapSha256": sha256(args.bootstrap),
        "bootstrapSize": args.bootstrap.stat().st_size,
        "schemaVersion": 1,
    }


def bootstrap_sidecar(args: argparse.Namespace) -> None:
    require_file(args.bootstrap, "Bootstrap")
    atomic_write(args.bootstrap_sig, (sha256(args.bootstrap) + "\n").encode("ascii"))


def finalize(args: argparse.Namespace) -> None:
    document = pair_document(args)
    atomic_write(
        args.bootstrap_sig,
        (str(document["bootstrapSha256"]) + "\n").encode("ascii"),
    )
    encoded = json.dumps(
        document, ensure_ascii=False, sort_keys=True, separators=(",", ":")
    ).encode("utf-8")
    atomic_write(args.receipt, encoded)


def verify(args: argparse.Namespace) -> None:
    require_file(args.aris, "Aris")
    require_file(args.bootstrap, "Bootstrap")
    require_file(args.bootstrap_sig, "Bootstrap integrity sidecar")
    require_file(args.receipt, "runtime-pair receipt")
    raw = args.receipt.read_bytes()
    if raw.startswith(b"\xef\xbb\xbf"):
        raise ValueError("runtime-pair receipt must not contain a UTF-8 BOM")
    try:
        text = raw.decode("utf-8", errors="strict")
        duplicate = False

        def reject_duplicates(pairs: list[tuple[str, object]]) -> dict[str, object]:
            nonlocal duplicate
            result: dict[str, object] = {}
            for key, value in pairs:
                if key in result:
                    duplicate = True
                result[key] = value
            return result

        document = json.loads(text, object_pairs_hook=reject_duplicates)
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ValueError("runtime-pair receipt is not strict UTF-8 JSON") from error
    if duplicate or not isinstance(document, dict) or set(document) != SCHEMA_FIELDS:
        raise ValueError("runtime-pair receipt schema is not closed")
    if (
        type(document["schemaVersion"]) is not int
        or document["schemaVersion"] != 1
        or type(document["arisSize"]) is not int
        or document["arisSize"] < 0
        or type(document["bootstrapSize"]) is not int
        or document["bootstrapSize"] < 0
        or any(
            not isinstance(document[field], str)
            for field in (
                "arisSha256", "bootstrapSha256", "buildId", "channel", "config",
                "packageKeyId", "publicKeyDigest", "sourceCommit",
            )
        )
    ):
        raise ValueError("runtime-pair receipt field types are invalid")
    canonical = json.dumps(
        document, ensure_ascii=False, sort_keys=True, separators=(",", ":")
    ).encode("utf-8")
    if raw != canonical:
        raise ValueError("runtime-pair receipt is not canonical UTF-8 JSON")
    expected = validate_metadata(args)
    for field in ("sourceCommit", "buildId", "channel", "config", "packageKeyId", "publicKeyDigest"):
        if document[field] != expected[field]:
            raise ValueError(f"runtime-pair receipt metadata mismatch: {field}")
    if document["schemaVersion"] != 1:
        raise ValueError("runtime-pair receipt schema version is unsupported")
    if document["arisSize"] != args.aris.stat().st_size or document["arisSha256"] != sha256(args.aris):
        raise ValueError("Aris bytes do not match the runtime-pair receipt")
    bootstrap_digest = sha256(args.bootstrap)
    if document["bootstrapSize"] != args.bootstrap.stat().st_size or document["bootstrapSha256"] != bootstrap_digest:
        raise ValueError("Bootstrap bytes do not match the runtime-pair receipt")
    with args.aris.open("rb") as aris_file:
        is_pe = aris_file.read(2) == b"MZ"
    if is_pe:
        bootstrap_image_digest, _ = signing_stable_pe_identity(args.bootstrap)
        if not contains_bytes(args.aris, bootstrap_image_digest.encode("ascii")):
            raise ValueError("Aris embedded Bootstrap identity does not match current bytes")
    if args.bootstrap_sig.read_bytes() != (bootstrap_digest + "\n").encode("ascii"):
        raise ValueError("Bootstrap integrity sidecar does not match current bytes")


def add_metadata(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--source-commit", required=True)
    parser.add_argument("--build-id", required=True)
    parser.add_argument("--channel", required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--key-id", required=True)
    parser.add_argument("--public-key-hex", required=True)


def main() -> int:
    parser = argparse.ArgumentParser()
    commands = parser.add_subparsers(dest="command", required=True)
    header = commands.add_parser("bootstrap-header")
    header.add_argument("--bootstrap", type=Path, required=True)
    header.add_argument("--output", type=Path, required=True)
    add_metadata(header)
    sidecar = commands.add_parser("bootstrap-sidecar")
    sidecar.add_argument("--bootstrap", type=Path, required=True)
    sidecar.add_argument("--bootstrap-sig", type=Path, required=True)
    finish = commands.add_parser("finalize")
    finish.add_argument("--aris", type=Path, required=True)
    finish.add_argument("--bootstrap", type=Path, required=True)
    finish.add_argument("--bootstrap-sig", type=Path, required=True)
    finish.add_argument("--receipt", type=Path, required=True)
    add_metadata(finish)
    check = commands.add_parser("verify")
    check.add_argument("--aris", type=Path, required=True)
    check.add_argument("--bootstrap", type=Path, required=True)
    check.add_argument("--bootstrap-sig", type=Path, required=True)
    check.add_argument("--receipt", type=Path, required=True)
    add_metadata(check)
    args = parser.parse_args()
    try:
        if args.command == "bootstrap-header":
            bootstrap_header(args)
        elif args.command == "bootstrap-sidecar":
            bootstrap_sidecar(args)
        elif args.command == "finalize":
            finalize(args)
        else:
            verify(args)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

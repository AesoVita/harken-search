#!/usr/bin/env python3
"""Build a deterministic, validated Aris release catalog from a source receipt."""

from __future__ import annotations

import argparse
from collections.abc import Mapping
import hashlib
import json
from pathlib import Path
import re
from urllib.parse import urlsplit


SCHEMA_VERSION = 1
REQUIRED_ASSET_ROLE = "windows_x64_update"
CHANNELS = frozenset({"stable", "testing"})
SEMVER = re.compile(r"(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\Z")
TOKEN = re.compile(r"[A-Za-z0-9][A-Za-z0-9._-]{0,127}\Z")
HEX64 = re.compile(r"[0-9a-f]{64}\Z")
HEX128 = re.compile(r"[0-9a-f]{128}\Z")
MUTABLE_TAGS = frozenset({"latest", "stable", "testing", "current", "head"})
CANONICAL_RELEASE_HOST = "github.com"
CANONICAL_RELEASE_PATH = "/xindong/harken-search/releases/download/"
LEGACY_RELEASE_PATH = "/xindong/aris/releases/download/"

SOURCE_KEYS = frozenset({
    "schema_version", "product", "channel", "release_revision", "release_tag",
    "version", "published_at", "ready", "primary_asset", "signature",
})
ASSET_KEYS = frozenset({"role", "file", "url", "size", "sha256"})
SIGNATURE_KEYS = frozenset({
    "algorithm", "encoding", "key_id", "manifest_url", "manifest_sha256",
    "signature_url", "signature_hex",
})


def _unique_pairs(pairs: list[tuple[str, object]]) -> dict[str, object]:
    result: dict[str, object] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def load_source(path: Path) -> dict[str, object]:
    try:
        payload = path.read_bytes().decode("utf-8")
        value = json.loads(
            payload,
            object_pairs_hook=_unique_pairs,
            parse_constant=lambda value: (_ for _ in ()).throw(
                ValueError(f"invalid JSON constant: {value}")
            ),
        )
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ValueError(f"source fixture is not strict UTF-8 JSON: {path}") from error
    if not isinstance(value, dict):
        raise ValueError("source fixture must contain one JSON object")
    return value


def _exact_keys(value: Mapping[str, object], expected: frozenset[str], label: str) -> None:
    if set(value) != expected:
        raise ValueError(f"{label} fields do not match the closed schema")


def _string(value: object, label: str, pattern: re.Pattern[str] | None = None) -> str:
    if not isinstance(value, str) or not value:
        raise ValueError(f"{label} must be a non-empty string")
    if pattern is not None and pattern.fullmatch(value) is None:
        raise ValueError(f"{label} has an invalid format")
    return value


def _lower_hex(value: object, label: str, pattern: re.Pattern[str]) -> str:
    text = _string(value, label)
    if pattern.fullmatch(text) is None:
        raise ValueError(f"{label} must be lowercase hexadecimal data")
    return text


def _immutable_url(value: object, label: str, release_tag: str) -> str:
    text = _string(value, label)
    parsed = urlsplit(text)
    if (
        parsed.scheme != "https"
        or not parsed.hostname
        or parsed.username is not None
        or parsed.password is not None
        or parsed.query
        or parsed.fragment
    ):
        raise ValueError(f"{label} must be an immutable HTTPS URL")
    if (
        parsed.hostname != CANONICAL_RELEASE_HOST
        or parsed.port is not None
        or not parsed.path.startswith((CANONICAL_RELEASE_PATH, LEGACY_RELEASE_PATH))
    ):
        raise ValueError(f"{label} must use the canonical GitHub release source")
    segments = [segment for segment in parsed.path.split("/") if segment]
    try:
        download_index = next(
            index for index, segment in enumerate(segments[:-1])
            if segment == "releases" and segments[index + 1] == "download"
        )
    except StopIteration as error:
        raise ValueError(f"{label} must use a versioned release URL") from error
    if len(segments) <= download_index + 3:
        raise ValueError(f"{label} is missing its immutable release tag or asset")
    tag = segments[download_index + 2]
    if tag != release_tag or tag.lower() in MUTABLE_TAGS or TOKEN.fullmatch(tag) is None:
        raise ValueError(f"{label} does not bind the immutable release tag")
    if any(segment in {"", ".", ".."} for segment in segments):
        raise ValueError(f"{label} contains an unsafe path")
    return text


def _resolve_asset_file(source_dir: Path, relative: object) -> Path:
    name = _string(relative, "primary_asset.file")
    candidate = Path(name)
    if candidate.is_absolute() or any(part in {"", ".", ".."} for part in candidate.parts):
        raise ValueError("primary_asset.file must be a safe relative path")
    resolved = (source_dir / candidate).resolve()
    try:
        resolved.relative_to(source_dir.resolve())
    except ValueError as error:
        raise ValueError("primary_asset.file escapes the source fixture") from error
    if not resolved.is_file() or resolved.stat().st_size <= 0:
        raise ValueError("required primary release asset is missing or empty")
    return resolved


def _canonical_material(source: Mapping[str, object], asset: dict[str, object]) -> dict[str, object]:
    return {
        "channel": source["channel"],
        "primary_asset": asset,
        "product": source["product"],
        "published_at": source["published_at"],
        "ready": source["ready"],
        "release_revision": source["release_revision"],
        "release_tag": source["release_tag"],
        "schema_version": source["schema_version"],
        "signature": source["signature"],
        "version": source["version"],
    }


def canonical_json_bytes(value: object) -> bytes:
    return json.dumps(
        value,
        ensure_ascii=False,
        sort_keys=True,
        separators=(",", ":"),
        allow_nan=False,
    ).encode("utf-8")


def _projection(source: Mapping[str, object], asset: dict[str, object]) -> dict[str, object]:
    return {
        "channel": source["channel"],
        "release_revision": source["release_revision"],
        "version": source["version"],
        "asset": {
            "url": asset["url"],
            "size": asset["size"],
            "sha256": asset["sha256"],
        },
    }


def build_catalog(source: Mapping[str, object], *, source_dir: Path) -> dict[str, object]:
    _exact_keys(source, SOURCE_KEYS, "source")
    if source["schema_version"] is not SCHEMA_VERSION:
        raise ValueError("source schema_version is unsupported")
    if source["product"] != "aris":
        raise ValueError("source product must be aris")
    channel = _string(source["channel"], "channel")
    if channel not in CHANNELS:
        raise ValueError("channel must be stable or testing")
    version = _string(source["version"], "version", SEMVER)
    release_revision = _string(source["release_revision"], "release_revision", TOKEN)
    release_tag = _string(source["release_tag"], "release_tag", TOKEN)
    if release_tag.lower() in MUTABLE_TAGS:
        raise ValueError("release_tag must be immutable")
    _string(source["published_at"], "published_at", re.compile(
        r"[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z\Z"
    ))
    if source["ready"] is not True:
        raise ValueError("release source is not ready")

    raw_asset = source["primary_asset"]
    if not isinstance(raw_asset, dict):
        raise ValueError("primary_asset is missing")
    _exact_keys(raw_asset, ASSET_KEYS, "primary_asset")
    if raw_asset["role"] != REQUIRED_ASSET_ROLE:
        raise ValueError("required primary asset role is missing")
    asset_file = _resolve_asset_file(source_dir, raw_asset["file"])
    size = raw_asset["size"]
    if isinstance(size, bool) or not isinstance(size, int) or size <= 0:
        raise ValueError("primary_asset.size must be a positive integer")
    if size != asset_file.stat().st_size:
        raise ValueError("primary_asset.size does not match the release asset")
    digest = _lower_hex(raw_asset["sha256"], "primary_asset.sha256", HEX64)
    actual_digest = hashlib.sha256(asset_file.read_bytes()).hexdigest()
    if digest != actual_digest:
        raise ValueError("primary_asset.sha256 does not match the release asset")
    asset_url = _immutable_url(raw_asset["url"], "primary_asset.url", release_tag)
    expected_name = f"Aris-{version}-win-x64-update.zip"
    if Path(urlsplit(asset_url).path).name != expected_name:
        raise ValueError("primary_asset.url is not bound to the release version")
    asset = {
        "role": REQUIRED_ASSET_ROLE,
        "url": asset_url,
        "size": size,
        "sha256": digest,
    }

    raw_signature = source["signature"]
    if not isinstance(raw_signature, dict):
        raise ValueError("signature metadata is missing")
    _exact_keys(raw_signature, SIGNATURE_KEYS, "signature")
    if raw_signature["algorithm"] != "ed25519":
        raise ValueError("signature.algorithm is unsupported")
    if raw_signature["encoding"] != "lowercase_hex":
        raise ValueError("signature.encoding is unsupported")
    _string(raw_signature["key_id"], "signature.key_id", TOKEN)
    _lower_hex(raw_signature["manifest_sha256"], "signature.manifest_sha256", HEX64)
    _lower_hex(raw_signature["signature_hex"], "signature.signature_hex", HEX128)
    manifest_url = _immutable_url(raw_signature["manifest_url"], "signature.manifest_url", release_tag)
    signature_url = _immutable_url(raw_signature["signature_url"], "signature.signature_url", release_tag)
    repository_paths = {urlsplit(url).path.split("/releases/download/", 1)[0]
                        for url in (asset_url, manifest_url, signature_url)}
    if len(repository_paths) != 1:
        raise ValueError("asset and signature metadata must use the same repository")
    if not Path(urlsplit(manifest_url).path).name.endswith(".json"):
        raise ValueError("signature.manifest_url must point to JSON manifest metadata")
    if not Path(urlsplit(signature_url).path).name.endswith(".sig"):
        raise ValueError("signature.signature_url must point to detached signature metadata")
    signature = dict(raw_signature)

    material_source = dict(source)
    material_source["signature"] = signature
    material = _canonical_material(material_source, asset)
    identity_digest = hashlib.sha256(canonical_json_bytes(material)).hexdigest()
    projection = _projection(source, asset)
    return {
        "schema_version": SCHEMA_VERSION,
        "product": "aris",
        "channel": channel,
        "release_revision": release_revision,
        "release_tag": release_tag,
        "version": version,
        "published_at": source["published_at"],
        "ready": True,
        "primary_asset": asset,
        "signature": signature,
        "identity": {
            "channel": channel,
            "release_revision": release_revision,
            "digest": identity_digest,
        },
        "projections": {
            "client": projection,
            "website": projection,
        },
    }


def build_catalog_bytes(source: Mapping[str, object], *, source_dir: Path) -> bytes:
    return canonical_json_bytes(build_catalog(source, source_dir=source_dir))


def client_projection(catalog: Mapping[str, object]) -> dict[str, object]:
    projections = catalog.get("projections")
    if not isinstance(projections, dict) or not isinstance(projections.get("client"), dict):
        raise ValueError("catalog client projection is missing")
    return dict(projections["client"])


def write_catalog(source_path: Path, output_path: Path) -> None:
    source = load_source(source_path)
    payload = build_catalog_bytes(source, source_dir=source_path.parent)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    temporary = output_path.with_name(output_path.name + ".tmp")
    try:
        temporary.write_bytes(payload)
        temporary.replace(output_path)
    finally:
        if temporary.exists():
            temporary.unlink()


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True, help="UTF-8 release source receipt JSON")
    parser.add_argument("--output", type=Path, required=True, help="deterministic catalog output JSON")
    return parser


def main() -> int:
    args = _parser().parse_args()
    try:
        write_catalog(args.source, args.output)
    except (OSError, ValueError) as error:
        print(f"error: {error}", flush=True)
        return 2
    print(args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

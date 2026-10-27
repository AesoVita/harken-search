#!/usr/bin/env python3
"""Prepare an isolated, QA-only Aris update demonstration fixture.

The helper deliberately owns only disposable files below ``DemoRoot``.  The
signed control-plane documents retain the normal GitHub identity; the local
loopback address is used solely as the transport mirror by the product's
existing URL resolver.
"""

from __future__ import annotations

import argparse
import base64
import binascii
from dataclasses import dataclass
from datetime import datetime, timezone
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import subprocess
import sys
from urllib.parse import urlsplit
import zipfile

from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric.ed25519 import (
    Ed25519PrivateKey,
    Ed25519PublicKey,
)

if __package__ in {None, ""}:
    import sys

    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tools.build_update_manifest import build_manifest, build_package_plan
from tools.release_update import build_deterministic_zip, verify_manifest_bytes


def _fixed_demo_root() -> Path:
    repo_root = Path(__file__).resolve().parents[1]
    result = subprocess.run(
        ["git", "rev-parse", "--path-format=absolute", "--git-common-dir"],
        cwd=repo_root,
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0 or not result.stdout.strip():
        raise RuntimeError("cannot resolve the Git common directory for DemoRoot")
    git_common_dir = Path(result.stdout.strip()).resolve()
    return git_common_dir.parent.parent / "aris-local-update-demo-20260803"


FIXED_DEMO_ROOT = _fixed_demo_root()
FIXED_MIRROR_BASE_URL = "http://127.0.0.1:4328/harken-search-demo/"
DEFAULT_REPOSITORY = "xindong/harken-search"
OLD_VERSION = "1.0.2"
NEW_VERSION = "1.0.3"
OLD_BUILD_ID = "local-demo-old-b76dd15d"
NEW_BUILD_ID = "local-demo-new-b76dd15d"
BOOTSTRAP_NAME = "HarkenSearchBootstrap.exe"
# Signed manifest asset names retain the release protocol's Aris prefix.
ZIP_NAME = f"Aris-{NEW_VERSION}-win-x64-update.zip"
MANIFEST_NAME = "manifest-win-x64-testing.json"
MANIFEST_SIG_NAME = MANIFEST_NAME + ".sig"
PACKAGE_NAME = "ArisBootstrap.package.json"
PACKAGE_SIG_NAME = PACKAGE_NAME + ".sig"
PAIR_RECEIPT_NAME = "harken-runtime-pair.json"
BOOTSTRAP_SIG_NAME = BOOTSTRAP_NAME + ".sig"
_SEMVER = re.compile(r"(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\Z")
_BUILD_ID = re.compile(r"[A-Za-z0-9_.-]{1,128}\Z")
_KEY_ID = re.compile(r"[A-Za-z0-9_.-]{1,64}\Z")
_SHA256 = re.compile(r"[0-9a-f]{64}\Z")


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _canonical_json(document: object) -> bytes:
    return json.dumps(document, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")


def _strict_json(raw: bytes, label: str) -> object:
    if raw.startswith(b"\xef\xbb\xbf"):
        raise ValueError(f"{label} must not contain a UTF-8 BOM")
    try:
        text = raw.decode("utf-8", errors="strict")
        duplicate = False

        def pairs(items: list[tuple[str, object]]) -> dict[str, object]:
            nonlocal duplicate
            result: dict[str, object] = {}
            for key, value in items:
                if key in result:
                    duplicate = True
                result[key] = value
            return result

        document = json.loads(text, object_pairs_hook=pairs)
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ValueError(f"{label} must be strict UTF-8 JSON") from error
    if duplicate:
        raise ValueError(f"{label} contains duplicate fields")
    if _canonical_json(document) != raw:
        raise ValueError(f"{label} is not canonical UTF-8 JSON")
    return document


def derive_public_key_hex(private_key_b64: str) -> str:
    """Derive the public key from a process-supplied raw Ed25519 seed."""
    if not isinstance(private_key_b64, str) or not private_key_b64:
        raise ValueError("demo private key is required from the process environment")
    try:
        raw = base64.b64decode(private_key_b64, validate=True)
    except (binascii.Error, ValueError) as error:
        raise ValueError("demo private key must be strict base64") from error
    if len(raw) != 32:
        raise ValueError("demo private key must decode to 32 bytes")
    public = Ed25519PrivateKey.from_private_bytes(raw).public_key().public_bytes(
        serialization.Encoding.Raw, serialization.PublicFormat.Raw
    )
    return public.hex()


@dataclass(frozen=True)
class DemoConfig:
    demo_root: Path
    mirror_base_url: str
    old_version: str
    new_version: str
    old_build_id: str
    new_build_id: str
    private_key_b64: str
    public_key_hex: str
    key_id: str
    source_commit: str
    repository: str = DEFAULT_REPOSITORY


@dataclass(frozen=True)
class DemoFixture:
    generation: str
    generation_dir: Path
    update_zip: Path
    manifest: Path
    manifest_signature: Path
    package_plan: Path
    package_signature: Path
    ledger: Path


def _version_tuple(value: str) -> tuple[int, int, int]:
    if not isinstance(value, str) or not _SEMVER.fullmatch(value):
        raise ValueError("demo versions must be strict numeric semver")
    return tuple(int(part) for part in value.split("."))  # type: ignore[return-value]


def _regular_file(path: Path, label: str) -> Path:
    path = Path(path)
    if not path.is_file() or path.is_symlink():
        raise ValueError(f"{label} must be one regular non-symlink file")
    if path.stat().st_size <= 0:
        raise ValueError(f"{label} must be nonempty")
    return path


def _under(path: Path, root: Path) -> bool:
    try:
        path.resolve().relative_to(root.resolve())
        return True
    except ValueError:
        return False


def _cuevi_protected_paths(repo_root: Path) -> set[Path]:
    """Protect Cuevi data wherever it actually lives, not a sibling-path guess."""
    paths: set[Path] = set()
    env_root = os.environ.get("CUEVI_ROOT")
    if env_root:
        root = Path(env_root).expanduser().resolve()
        paths.add(root)
        paths.add(root / "dev.db")
    sibling = Path(repo_root).resolve().parent / "cuevi" / ".cuevi"
    paths.add(sibling)
    paths.add(sibling / "dev.db")
    return {item for item in paths if str(item)}


def _reject_reparse_alias(path: Path, label: str) -> None:
    """Reject symlink/junction/reparse components before resolving a boundary."""
    candidate = Path(path).expanduser()
    if not candidate.is_absolute():
        candidate = Path.cwd() / candidate
    for component in candidate.parents[::-1] + (candidate,):
        try:
            info = component.lstat()
        except FileNotFoundError:
            continue
        if stat.S_ISLNK(info.st_mode) or (
            getattr(info, "st_file_attributes", 0) & 0x400
        ):
            raise ValueError(f"{label} must not contain symlink/junction/reparse aliases")


def _validate_mirror(url: str) -> None:
    if url != FIXED_MIRROR_BASE_URL:
        raise ValueError("demo mirror must be the exact loopback URL http://127.0.0.1:4328/harken-search-demo/")
    parsed = urlsplit(url)
    if (
        parsed.scheme != "http"
        or parsed.hostname != "127.0.0.1"
        or parsed.port != 4328
        or parsed.path != "/harken-search-demo/"
        or parsed.query
        or parsed.fragment
        or parsed.username
        or parsed.password
    ):
        raise ValueError("demo mirror must be the exact loopback URL http://127.0.0.1:4328/harken-search-demo/")


def validate_config(
    config: DemoConfig, *, repo_root: Path | None = None,
    require_signing_key: bool = True,
) -> None:
    _validate_mirror(config.mirror_base_url)
    old = _version_tuple(config.old_version)
    new = _version_tuple(config.new_version)
    if new <= old:
        raise ValueError("demo new version must be newer than old version")
    if config.old_version != OLD_VERSION or config.new_version != NEW_VERSION:
        raise ValueError("demo versions must match the approved old/new identities")
    if not _BUILD_ID.fullmatch(config.old_build_id) or not _BUILD_ID.fullmatch(config.new_build_id):
        raise ValueError("demo build ids are malformed")
    if config.old_build_id == config.new_build_id:
        raise ValueError("demo old and new build ids must differ")
    if config.old_build_id != OLD_BUILD_ID or config.new_build_id != NEW_BUILD_ID:
        raise ValueError("demo build ids must match the approved old/new identities")
    if not re.fullmatch(r"[0-9a-f]{40}", config.source_commit):
        raise ValueError("demo source commit must be 40 lowercase hex characters")
    if not _KEY_ID.fullmatch(config.key_id):
        raise ValueError("demo key id is malformed")
    if not re.fullmatch(r"[0-9a-f]{64}", config.public_key_hex) or set(config.public_key_hex) == {"0"}:
        raise ValueError("demo public key must be a nonzero lowercase 32-byte hex value")
    expected_public = os.environ.get("ARIS_UPDATE_LOCAL_DEMO_PUBLIC_KEY_HEX")
    expected_key_id = os.environ.get("ARIS_UPDATE_LOCAL_DEMO_KEY_ID")
    if not expected_public or not expected_key_id:
        raise ValueError("demo verification identity must be supplied by the process environment")
    if config.public_key_hex != expected_public or config.key_id != expected_key_id:
        raise ValueError("demo verification identity does not match the process environment")
    if require_signing_key:
        expected_private = os.environ.get("ARIS_UPDATE_LOCAL_DEMO_PRIVATE_KEY_B64")
        if not expected_private:
            raise ValueError("demo signing identity must be supplied by the process environment")
        if derive_public_key_hex(config.private_key_b64) != config.public_key_hex:
            raise ValueError("demo public key does not match the process-supplied private key")
        if config.private_key_b64 != expected_private:
            raise ValueError("demo signing identity does not match the process environment")
        if os.environ.get("ARIS_UPDATE_SIGNING_KEY_B64") != config.private_key_b64:
            raise ValueError("canonical signing key does not match the demo signing identity")
        if os.environ.get("ARIS_UPDATE_PACKAGE_KEY_ID") != config.key_id:
            raise ValueError("canonical package key id does not match the demo key id")
    if config.key_id != expected_key_id:
        raise ValueError("demo key id does not match the process environment")
    if not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", config.repository):
        raise ValueError("demo repository must be owner/name")

    _reject_reparse_alias(Path(config.demo_root), "demo root")
    root = Path(config.demo_root).expanduser().resolve()
    if root == Path(root.anchor):
        raise ValueError("demo root may not be a filesystem root")
    repo = Path(repo_root or Path(__file__).resolve().parents[1]).resolve()
    forbidden = {
        repo,
        repo / "dist_qa",
        repo / "dist_qa" / "Airs",
        repo / "installed",
        repo / "portable",
        repo / "source-audio",
        *_cuevi_protected_paths(repo),
    }
    if any(_under(root, item) or _under(item, root) for item in forbidden):
        raise ValueError("demo root overlaps the repository, fixed QA output, user database, or source audio")
    appdata = os.environ.get("APPDATA")
    local_appdata = os.environ.get("LOCALAPPDATA")
    if appdata and (
        _under(root, Path(appdata) / "AuroAudio")
        or _under(Path(appdata) / "AuroAudio", root)
    ):
        raise ValueError("demo root overlaps the roaming user-data root")
    if local_appdata and (
        _under(root, Path(local_appdata) / "Auro" / "Database")
        or _under(Path(local_appdata) / "Auro" / "Database", root)
    ):
        raise ValueError("demo root overlaps the local database root")


def _pair_artifacts(pair_dir: Path) -> dict[str, Path]:
    pair_dir = Path(pair_dir)
    if not pair_dir.is_dir() or pair_dir.is_symlink():
        raise ValueError("runtime pair must be one regular directory")
    paths = {
        "aris": pair_dir / "HarkenSearch.exe",
        "arisSignature": pair_dir / "HarkenSearch.exe.sig",
        "bootstrap": pair_dir / BOOTSTRAP_NAME,
        "bootstrapSignature": pair_dir / BOOTSTRAP_SIG_NAME,
        "receipt": pair_dir / PAIR_RECEIPT_NAME,
    }
    for label, path in paths.items():
        _regular_file(path, f"runtime pair {label}")
    return paths


def _pair_tree_files(pair_dir: Path) -> list[Path]:
    """Return the complete regular-file payload of a verified pair directory."""
    pair_dir = Path(pair_dir)
    files: list[Path] = []
    for entry in pair_dir.rglob("*"):
        if entry.is_symlink():
            raise ValueError("runtime pair must not contain symlinks or reparse aliases")
        if entry.is_dir():
            continue
        if not entry.is_file():
            raise ValueError("runtime pair contains a non-regular entry")
        relative = PurePosixPath(entry.relative_to(pair_dir).as_posix())
        if relative.is_absolute() or ".." in relative.parts:
            raise ValueError("runtime pair contains an unsafe relative path")
        files.append(entry)
    return sorted(files, key=lambda path: path.relative_to(pair_dir).as_posix().casefold())


def _verify_runtime_pair(
    pair_dir: Path, config: DemoConfig, *, build_id: str,
) -> tuple[dict[str, Path], dict[str, object]]:
    paths = _pair_artifacts(pair_dir)
    verifier = Path(__file__).with_name("generate_runtime_pair.py")
    result = subprocess.run(
        [
            sys.executable,
            "-B",
            str(verifier),
            "verify",
            "--aris", str(paths["aris"]),
            "--bootstrap", str(paths["bootstrap"]),
            "--bootstrap-sig", str(paths["bootstrapSignature"]),
            "--receipt", str(paths["receipt"]),
            "--source-commit", config.source_commit,
            "--build-id", build_id,
            "--channel", "testing",
            "--config", "Release",
            "--key-id", config.key_id,
            "--public-key-hex", config.public_key_hex,
        ],
        check=False,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    if result.returncode != 0:
        raise ValueError(f"runtime pair {build_id} failed exact receipt verification")
    receipt = _strict_json(paths["receipt"].read_bytes(), "runtime pair receipt")
    if not isinstance(receipt, dict):
        raise ValueError("runtime pair receipt must be an object")
    if (
        receipt.get("packageKeyId") != config.key_id
        or receipt.get("publicKeyDigest")
        != hashlib.sha256(bytes.fromhex(config.public_key_hex)).hexdigest()
    ):
        raise ValueError("runtime pair key identity is not bound to the demo identity")
    return paths, receipt


def _pair_hashes(paths: dict[str, Path]) -> dict[str, str]:
    return {
        "arisSha256": _sha256(paths["aris"]),
        "arisSignatureSha256": _sha256(paths["arisSignature"]),
        "bootstrapSha256": _sha256(paths["bootstrap"]),
        "bootstrapSignatureSha256": _sha256(paths["bootstrapSignature"]),
        "receiptSha256": _sha256(paths["receipt"]),
    }


def _pair_tree_hashes(pair_dir: Path) -> dict[str, str]:
    root = Path(pair_dir)
    return {
        path.relative_to(root).as_posix(): _sha256(path)
        for path in _pair_tree_files(root)
    }


def _generation_id(
    config: DemoConfig,
    old_pair: dict[str, Path],
    new_pair: dict[str, Path],
) -> str:
    identity = {
        "sourceCommit": config.source_commit,
        "oldVersion": config.old_version,
        "newVersion": config.new_version,
        "oldBuildId": config.old_build_id,
        "newBuildId": config.new_build_id,
        "keyId": config.key_id,
        "publicKeyDigest": hashlib.sha256(bytes.fromhex(config.public_key_hex)).hexdigest(),
        "oldPair": {**_pair_hashes(old_pair), "files": _pair_tree_hashes(old_pair["aris"].parent)},
        "newPair": {**_pair_hashes(new_pair), "files": _pair_tree_hashes(new_pair["aris"].parent)},
        "mirrorBaseUrl": config.mirror_base_url,
        "repository": config.repository,
    }
    return hashlib.sha256(_canonical_json(identity)).hexdigest()


def _copy_tree_file(source: Path, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, destination)


def _write_ledger(path: Path, document: dict[str, object]) -> None:
    path.write_bytes(_canonical_json(document))


def _artifact_map(root: Path) -> dict[str, Path]:
    return {
        "manifest": root / MANIFEST_NAME,
        "manifestSignature": root / MANIFEST_SIG_NAME,
        "zip": root / ZIP_NAME,
        "packagePlan": root / PACKAGE_NAME,
        "packageSignature": root / PACKAGE_SIG_NAME,
    }


def _verify_package(plan_path: Path, signature_path: Path, asset_path: Path, config: DemoConfig) -> dict[str, object]:
    raw = _regular_file(plan_path, "package plan").read_bytes()
    document = _strict_json(raw, "package plan")
    if not isinstance(document, dict):
        raise ValueError("package plan must be an object")
    expected_keys = {
        "asset_sha256", "asset_size", "build_id", "channel", "entrypoint",
        "inventory", "key_id", "product", "removals", "schema_version", "version",
    }
    if (
        set(document) != expected_keys
        or document.get("channel") != "testing"
        or document.get("version") != config.new_version
    ):
        raise ValueError("package plan schema or identity is not bound")
    if document.get("build_id") != config.new_build_id or document.get("key_id") != config.key_id:
        raise ValueError("package plan build/key identity is not bound")
    if document.get("removals") != [] or document.get("entrypoint") != "HarkenSearch.exe":
        raise ValueError("demo package plan must not remove files")
    inventory = document.get("inventory")
    if not isinstance(inventory, list):
        raise ValueError("package inventory is malformed")
    if document.get("asset_sha256") != _sha256(asset_path) or document.get("asset_size") != asset_path.stat().st_size:
        raise ValueError("package plan asset binding drifted")
    signature = _regular_file(signature_path, "package signature").read_bytes()
    if not re.fullmatch(rb"[0-9a-f]{128}", signature):
        raise ValueError("package signature encoding is invalid")
    try:
        Ed25519PublicKey.from_public_bytes(bytes.fromhex(config.public_key_hex)).verify(
            bytes.fromhex(signature.decode("ascii")), raw
        )
    except (InvalidSignature, ValueError) as error:
        raise ValueError("package plan signature is invalid") from error
    seen: set[str] = set()
    try:
        with zipfile.ZipFile(asset_path) as archive:
            names = archive.namelist()
            normalized_names = [name.casefold() for name in names]
            if len(normalized_names) != len(set(normalized_names)):
                raise ValueError("demo ZIP contains duplicate members")
            if names.count(BOOTSTRAP_NAME) != 1:
                raise ValueError("demo ZIP must contain exactly one Bootstrap")
            for item in inventory:
                if not isinstance(item, dict) or set(item) != {"byte_size", "relative_path", "sha256"}:
                    raise ValueError("package inventory member is malformed")
                relative = item["relative_path"]
                if not isinstance(relative, str) or not relative or "\\" in relative:
                    raise ValueError("package inventory path is not normalized")
                normalized = PurePosixPath(relative)
                if normalized.is_absolute() or ".." in normalized.parts or relative.casefold() in seen:
                    raise ValueError("package inventory path is ambiguous")
                seen.add(relative.casefold())
                payload = archive.read(relative)
                if (
                    type(item["byte_size"]) is not int
                    or item["byte_size"] != len(payload)
                    or item["sha256"] != hashlib.sha256(payload).hexdigest()
                    or not _SHA256.fullmatch(str(item["sha256"]))
                ):
                    raise ValueError("package inventory member drifted")
            expected_payload = {name.casefold() for name in names}
            if expected_payload != seen:
                raise ValueError("package inventory does not cover the ZIP payload")
    except (zipfile.BadZipFile, KeyError) as error:
        raise ValueError("demo update ZIP is malformed") from error
    return document


def verify_generation(generation_dir: Path, config: DemoConfig) -> dict[str, object]:
    """Verify every signed artifact and the non-secret generation ledger."""
    validate_config(config, require_signing_key=False)
    generation_dir = Path(generation_dir)
    ledger_path = _regular_file(generation_dir / "generation.json", "generation ledger")
    ledger = _strict_json(ledger_path.read_bytes(), "generation ledger")
    if not isinstance(ledger, dict):
        raise ValueError("generation ledger must be an object")
    expected_ledger_keys = {
        "schemaVersion", "generation", "sourceCommit", "oldVersion",
        "newVersion", "oldBuildId", "newBuildId", "keyId",
        "publicKeyDigest", "mirrorBaseUrl", "repository", "artifacts",
        "pair",
    }
    if set(ledger) != expected_ledger_keys or ledger.get("schemaVersion") != 1:
        raise ValueError("generation ledger schema is not closed")
    expected_generation = ledger.get("generation")
    directory_generation = generation_dir.name
    if directory_generation.startswith(".generation-") and directory_generation.endswith(".tmp"):
        directory_generation = directory_generation[len(".generation-") : -len(".tmp")]
    elif directory_generation == "harken-search-demo":
        directory_generation = expected_generation
    if not isinstance(expected_generation, str) or expected_generation != directory_generation:
        raise ValueError("generation identity is stale")
    expected_identity = {
        "sourceCommit": config.source_commit,
        "oldVersion": config.old_version,
        "newVersion": config.new_version,
        "oldBuildId": config.old_build_id,
        "newBuildId": config.new_build_id,
        "keyId": config.key_id,
        "publicKeyDigest": hashlib.sha256(bytes.fromhex(config.public_key_hex)).hexdigest(),
        "mirrorBaseUrl": config.mirror_base_url,
        "repository": config.repository,
    }
    if any(ledger.get(key) != value for key, value in expected_identity.items()):
        raise ValueError("generation provenance is stale or mismatched")
    old_pair, _ = _verify_runtime_pair(
        generation_dir / "pair" / "old", config, build_id=config.old_build_id
    )
    new_pair, _ = _verify_runtime_pair(
        generation_dir / "pair" / "new", config, build_id=config.new_build_id
    )
    if (
        not isinstance(expected_generation, str)
        or not _SHA256.fullmatch(expected_generation)
        or expected_generation != _generation_id(config, old_pair, new_pair)
    ):
        raise ValueError("generation identity is stale or mismatched")
    pair_ledger = ledger.get("pair")
    if (
        not isinstance(pair_ledger, dict)
        or set(pair_ledger) != {"old", "new"}
        or pair_ledger.get("old") != {
            **_pair_hashes(old_pair),
            "files": _pair_tree_hashes(generation_dir / "pair" / "old"),
        }
        or pair_ledger.get("new") != {
            **_pair_hashes(new_pair),
            "files": _pair_tree_hashes(generation_dir / "pair" / "new"),
        }
    ):
        raise ValueError("generation runtime-pair ledger drifted")
    artifacts = _artifact_map(generation_dir)
    for path in artifacts.values():
        _regular_file(path, "generation artifact")
    manifest_raw = artifacts["manifest"].read_bytes()
    manifest = verify_manifest_bytes(
        manifest_raw,
        artifacts["manifestSignature"].read_bytes(),
        artifacts["zip"].read_bytes(),
        bytes.fromhex(config.public_key_hex),
        channel="testing",
        version=config.new_version,
        repository=config.repository,
    )
    if _canonical_json(manifest) != manifest_raw:
        raise ValueError("manifest bytes are not canonical")
    package = _verify_package(
        artifacts["packagePlan"], artifacts["packageSignature"],
        artifacts["zip"], config,
    )
    with zipfile.ZipFile(artifacts["zip"]) as archive:
        try:
            zip_aris = archive.read("HarkenSearch.exe")
            zip_bootstrap = archive.read(BOOTSTRAP_NAME)
        except KeyError as error:
            raise ValueError("demo ZIP is missing its runtime pair") from error
    new_aris_hash = _sha256(new_pair["aris"])
    new_bootstrap_hash = _sha256(new_pair["bootstrap"])
    package_aris = [
        item for item in package["inventory"]
        if isinstance(item, dict)
        and str(item.get("relative_path", "")).casefold() == "harkensearch.exe"
    ]
    published_bootstrap = _regular_file(
        generation_dir / BOOTSTRAP_NAME, "published demo Bootstrap"
    )
    if (
        hashlib.sha256(zip_aris).hexdigest() != new_aris_hash
        or hashlib.sha256(zip_bootstrap).hexdigest() != new_bootstrap_hash
        or _sha256(published_bootstrap) != new_bootstrap_hash
        or len(package_aris) != 1
        or package_aris[0].get("sha256") != new_aris_hash
        or package_aris[0].get("byte_size") != new_pair["aris"].stat().st_size
    ):
        raise ValueError("signed package runtime pair is not generation-bound")
    ledger_artifacts = ledger.get("artifacts")
    expected_artifact_keys = {
        "manifestSha256", "manifestSignatureSha256", "zipSha256",
        "packagePlanSha256", "packageSignatureSha256",
    }
    if (
        not isinstance(ledger_artifacts, dict)
        or set(ledger_artifacts) != expected_artifact_keys
    ):
        raise ValueError("generation ledger artifacts are missing")
    expected_hashes = {
        "manifestSha256": _sha256(artifacts["manifest"]),
        "manifestSignatureSha256": _sha256(artifacts["manifestSignature"]),
        "zipSha256": _sha256(artifacts["zip"]),
        "packagePlanSha256": _sha256(artifacts["packagePlan"]),
        "packageSignatureSha256": _sha256(artifacts["packageSignature"]),
    }
    if any(ledger_artifacts.get(key) != value for key, value in expected_hashes.items()):
        raise ValueError("generation artifact ledger drifted")
    mirror_latest = generation_dir / "releases" / "latest" / "download"
    mirror_release = generation_dir / "releases" / "download" / f"v{config.new_version}"
    if (mirror_latest / MANIFEST_NAME).read_bytes() != manifest_raw or (mirror_latest / MANIFEST_SIG_NAME).read_bytes() != artifacts["manifestSignature"].read_bytes():
        raise ValueError("latest mirror pair drifted")
    for source in artifacts.values():
        if (mirror_release / source.name).read_bytes() != source.read_bytes():
            raise ValueError("release mirror artifact drifted")
    return ledger


def _atomic_replace_generation(staged: Path, current: Path) -> None:
    current.parent.mkdir(parents=True, exist_ok=True)
    backup = current.with_name(current.name + ".previous")
    if backup.exists():
        shutil.rmtree(backup)
    if current.exists():
        os.replace(current, backup)
    try:
        os.replace(staged, current)
    except BaseException:
        if backup.exists() and not current.exists():
            os.replace(backup, current)
        raise
    if backup.exists():
        # The new generation is already committed at this point.  A failure
        # to remove the recoverable previous generation must not turn a
        # successful publish into a reported failure; retain it for audit.
        try:
            shutil.rmtree(backup)
        except OSError:
            pass


def prepare_demo_fixture(
    config: DemoConfig, *, old_pair_dir: Path, new_pair_dir: Path,
    payload_dir: Path,
) -> DemoFixture:
    validate_config(config)
    old_pair, _ = _verify_runtime_pair(
        old_pair_dir, config, build_id=config.old_build_id
    )
    new_pair, _ = _verify_runtime_pair(
        new_pair_dir, config, build_id=config.new_build_id
    )
    old_aris = old_pair["aris"]
    new_aris = new_pair["aris"]
    bootstrap = new_pair["bootstrap"]
    payload_dir = Path(payload_dir)
    if not payload_dir.is_dir() or payload_dir.is_symlink():
        raise ValueError("payload directory must be a regular directory")
    repo_root = Path(__file__).resolve().parents[1]
    forbidden_inputs = {
        repo_root / "dist_qa",
        repo_root / "dist_qa" / "Airs",
        *_cuevi_protected_paths(repo_root),
    }
    if any(
        _under(candidate, Path(config.demo_root))
        or any(_under(candidate, forbidden) for forbidden in forbidden_inputs)
        for candidate in (Path(old_pair_dir), Path(new_pair_dir), payload_dir)
    ):
        raise ValueError("demo inputs must not be sourced from DemoRoot")
    if _sha256(old_aris) == _sha256(new_aris):
        raise ValueError("old and new demo Aris bytes must be distinct")
    for entry in payload_dir.rglob("*"):
        if entry.is_symlink():
            raise ValueError("demo payload must not contain symlinks")
    payload_aris = _regular_file(payload_dir / "HarkenSearch.exe", "demo payload Aris")
    if _sha256(payload_aris) != _sha256(new_aris):
        raise ValueError("demo payload Aris must match the verified new runtime pair")
    generation = _generation_id(config, old_pair, new_pair)
    http_root = Path(config.demo_root) / "http-root"
    staging = http_root / f".generation-{generation}.tmp"
    current = http_root / "harken-search-demo"
    if staging.exists():
        shutil.rmtree(staging)
    staging.mkdir(parents=True, exist_ok=True)
    try:
        paths = _artifact_map(staging)
        for label, pair in (("old", old_pair), ("new", new_pair)):
            pair_root = pair["aris"].parent
            for source in _pair_tree_files(pair_root):
                _copy_tree_file(
                    source,
                    staging / "pair" / label / source.relative_to(pair_root),
                )
        _copy_tree_file(bootstrap, staging / BOOTSTRAP_NAME)
        work_payload = staging / "payload"
        shutil.copytree(payload_dir, work_payload)
        for name, source in (
            ("HarkenSearch.exe.sig", new_pair["arisSignature"]),
            (BOOTSTRAP_SIG_NAME, new_pair["bootstrapSignature"]),
            (PAIR_RECEIPT_NAME, new_pair["receipt"]),
        ):
            _copy_tree_file(source, work_payload / name)
        build_deterministic_zip(work_payload, staging / BOOTSTRAP_NAME, paths["zip"])
        previous_key = os.environ.get("ARIS_UPDATE_SIGNING_KEY_B64")
        previous_id = os.environ.get("ARIS_UPDATE_PACKAGE_KEY_ID")
        os.environ["ARIS_UPDATE_SIGNING_KEY_B64"] = config.private_key_b64
        os.environ["ARIS_UPDATE_PACKAGE_KEY_ID"] = config.key_id
        try:
            build_package_plan(
                channel="testing", version=config.new_version, build_id=config.new_build_id,
                asset_path=paths["zip"],
                inventory=[
                    {"relative_path": p.relative_to(work_payload).as_posix(), "source_path": p}
                    for p in sorted(work_payload.rglob("*")) if p.is_file()
                ] + [{"relative_path": BOOTSTRAP_NAME, "source_path": bootstrap}],
                output_dir=staging, key_id=config.key_id,
            )
            build_manifest(
                channel="testing", version=config.new_version, asset_path=paths["zip"],
                repository=config.repository, build_id=config.new_build_id,
                published_at=datetime.now(timezone.utc).replace(microsecond=0).strftime("%Y-%m-%dT%H:%M:%SZ"),
                allow_stable=False, output_dir=staging,
            )
        finally:
            if previous_key is None:
                os.environ.pop("ARIS_UPDATE_SIGNING_KEY_B64", None)
            else:
                os.environ["ARIS_UPDATE_SIGNING_KEY_B64"] = previous_key
            if previous_id is None:
                os.environ.pop("ARIS_UPDATE_PACKAGE_KEY_ID", None)
            else:
                os.environ["ARIS_UPDATE_PACKAGE_KEY_ID"] = previous_id
        paths = _artifact_map(staging)
        mirror_latest = staging / "releases" / "latest" / "download"
        mirror_release = staging / "releases" / "download" / f"v{config.new_version}"
        for key in ("manifest", "manifestSignature"):
            _copy_tree_file(paths[key], mirror_latest / paths[key].name)
        for source in paths.values():
            _copy_tree_file(source, mirror_release / source.name)
        ledger = {
            "schemaVersion": 1,
            "generation": generation,
            "sourceCommit": config.source_commit,
            "oldVersion": config.old_version,
            "newVersion": config.new_version,
            "oldBuildId": config.old_build_id,
            "newBuildId": config.new_build_id,
            "keyId": config.key_id,
            "publicKeyDigest": hashlib.sha256(bytes.fromhex(config.public_key_hex)).hexdigest(),
            "mirrorBaseUrl": config.mirror_base_url,
            "repository": config.repository,
            "artifacts": {
                "manifestSha256": _sha256(paths["manifest"]),
                "manifestSignatureSha256": _sha256(paths["manifestSignature"]),
                "zipSha256": _sha256(paths["zip"]),
                "packagePlanSha256": _sha256(paths["packagePlan"]),
                "packageSignatureSha256": _sha256(paths["packageSignature"]),
            },
            "pair": {
                "old": {
                    **_pair_hashes(old_pair),
                    "files": _pair_tree_hashes(old_pair["aris"].parent),
                },
                "new": {
                    **_pair_hashes(new_pair),
                    "files": _pair_tree_hashes(new_pair["aris"].parent),
                },
            },
        }
        _write_ledger(staging / "generation.json", ledger)
        secret_values = (
            config.private_key_b64.encode("ascii"),
            base64.b64decode(config.private_key_b64, validate=True),
        )
        for artifact in staging.rglob("*"):
            if not artifact.is_file():
                continue
            payload = artifact.read_bytes()
            if any(secret and secret in payload for secret in secret_values):
                raise ValueError("demo private key leaked into a fixture artifact")
        verify_generation(staging, config)
        _atomic_replace_generation(staging, current)
        return DemoFixture(
            generation=generation, generation_dir=current,
            update_zip=current / ZIP_NAME, manifest=current / MANIFEST_NAME,
            manifest_signature=current / MANIFEST_SIG_NAME,
            package_plan=current / PACKAGE_NAME,
            package_signature=current / PACKAGE_SIG_NAME,
            ledger=current / "generation.json",
        )
    except BaseException:
        if staging.exists():
            shutil.rmtree(staging)
        raise


def cancel_demo(config: DemoConfig) -> None:
    """Remove only incomplete disposable staging data for a cancelled demo."""
    validate_config(config, require_signing_key=False)
    root = Path(config.demo_root) / "http-root"
    if root.is_dir():
        for path in root.glob(".generation-*.tmp"):
            if path.is_dir() and not path.is_symlink():
                shutil.rmtree(path)


def create_loopback_server(config: DemoConfig) -> ThreadingHTTPServer:
    """Create the fixed read-only loopback mirror after full generation verification."""
    validate_config(config, require_signing_key=False)
    http_root = Path(config.demo_root) / "http-root"
    current = http_root / "harken-search-demo"
    verify_generation(current, config)

    artifact_root = current
    allowed_paths = {
        f"/harken-search-demo/releases/latest/download/{MANIFEST_NAME}": artifact_root / "releases" / "latest" / "download" / MANIFEST_NAME,
        f"/harken-search-demo/releases/latest/download/{MANIFEST_SIG_NAME}": artifact_root / "releases" / "latest" / "download" / MANIFEST_SIG_NAME,
    }
    for name in (MANIFEST_NAME, MANIFEST_SIG_NAME, ZIP_NAME, PACKAGE_NAME, PACKAGE_SIG_NAME):
        allowed_paths[f"/harken-search-demo/releases/download/v{config.new_version}/{name}"] = (
            artifact_root / "releases" / "download" / f"v{config.new_version}" / name
        )
    pinned_payloads = {
        path: _regular_file(target, "mirror artifact").read_bytes()
        for path, target in allowed_paths.items()
    }

    class DemoHandler(BaseHTTPRequestHandler):
        def __init__(self, *args: object, **kwargs: object) -> None:
            super().__init__(*args, **kwargs)

        def do_GET(self) -> None:  # noqa: N802
            request = urlsplit(self.path)
            target = allowed_paths.get(request.path) if not request.query and not request.fragment else None
            if target is None:
                self.send_error(404)
                return
            try:
                verify_generation(current, config)
                payload = _regular_file(target, "mirror artifact").read_bytes()
                if payload != pinned_payloads[request.path]:
                    raise ValueError("mirror artifact changed after server start")
            except ValueError:
                self.send_error(409)
                return
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(len(payload)))
            self.end_headers()
            self.wfile.write(payload)

        def do_HEAD(self) -> None:  # noqa: N802
            self.do_GET()

        def log_message(self, _format: str, *_args: object) -> None:
            return

    server = ThreadingHTTPServer(("127.0.0.1", 4328), DemoHandler)
    server.daemon_threads = True
    return server


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--demo-root", type=Path, default=FIXED_DEMO_ROOT)
    parser.add_argument("--mirror-base-url", default=FIXED_MIRROR_BASE_URL)
    parser.add_argument("--old-version", default=OLD_VERSION)
    parser.add_argument("--new-version", default=NEW_VERSION)
    parser.add_argument("--old-build-id", default=OLD_BUILD_ID)
    parser.add_argument("--new-build-id", default=NEW_BUILD_ID)
    parser.add_argument("--old-pair-dir", type=Path)
    parser.add_argument("--new-pair-dir", type=Path)
    parser.add_argument("--payload-dir", type=Path)
    parser.add_argument("--source-commit", default="")
    parser.add_argument("--repository", default=DEFAULT_REPOSITORY)
    parser.add_argument("--key-id", default="")
    parser.add_argument("--public-key-hex", default="")
    parser.add_argument("--cancel", action="store_true")
    parser.add_argument("--serve", action="store_true")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = _parser().parse_args(argv)
    if args.source_commit:
        source_commit = args.source_commit
    else:
        source_commit = subprocess.run(
            ["git", "rev-parse", "HEAD"], check=True, text=True,
            encoding="utf-8", stdout=subprocess.PIPE,
        ).stdout.strip()
    key_id = args.key_id or os.environ.get("ARIS_UPDATE_LOCAL_DEMO_KEY_ID", "")
    private = os.environ.get("ARIS_UPDATE_LOCAL_DEMO_PRIVATE_KEY_B64", "")
    public = args.public_key_hex or os.environ.get("ARIS_UPDATE_LOCAL_DEMO_PUBLIC_KEY_HEX", "")
    config = DemoConfig(
        demo_root=args.demo_root, mirror_base_url=args.mirror_base_url,
        old_version=args.old_version, new_version=args.new_version,
        old_build_id=args.old_build_id, new_build_id=args.new_build_id,
        private_key_b64=private, public_key_hex=public, key_id=key_id,
        source_commit=source_commit, repository=args.repository,
    )
    validate_config(
        config,
        repo_root=Path(__file__).resolve().parents[1],
        require_signing_key=not (args.cancel or args.serve),
    )
    if args.cancel:
        cancel_demo(config)
        return 0
    if args.serve:
        server = create_loopback_server(config)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass
        finally:
            server.server_close()
        return 0
    required = (args.old_pair_dir, args.new_pair_dir, args.payload_dir)
    if any(value is None for value in required):
        raise ValueError("--old-pair-dir, --new-pair-dir and --payload-dir are required")
    fixture = prepare_demo_fixture(
        config,
        old_pair_dir=args.old_pair_dir,
        new_pair_dir=args.new_pair_dir,
        payload_dir=args.payload_dir,
    )
    print(json.dumps({"generation": fixture.generation, "generationDir": str(fixture.generation_dir), "zipSha256": _sha256(fixture.update_zip)}, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError) as error:
        print(f"local demo refused: {error}", file=__import__("sys").stderr)
        raise SystemExit(2)

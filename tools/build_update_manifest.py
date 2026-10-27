#!/usr/bin/env python3
"""Build and sign one immutable Aris update-channel manifest."""

from __future__ import annotations

import argparse
import base64
import binascii
import hashlib
import json
import os
import re
import secrets
from pathlib import Path
import zipfile

from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey


_SEMVER = re.compile(r"(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\Z")
_REPOSITORY = re.compile(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+\Z")
_PUBLISHED_AT = re.compile(r"[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z\Z")
_CHANNELS = {"testing", "stable"}
_KEY_ENV = "ARIS_UPDATE_SIGNING_KEY_B64"
_PACKAGE_KEY_ID_ENV = "ARIS_UPDATE_PACKAGE_KEY_ID"
PUBLIC_REPOSITORY = "xindong/harken-search"
LEGACY_UPDATE_REPOSITORY = "xindong/aris"


def manifest_filename(repository: str, channel: str) -> str:
    if repository == PUBLIC_REPOSITORY and channel == "stable":
        return "manifest-harken-search-win-x64-stable.json"
    return f"manifest-win-x64-{channel}.json"


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _load_private_key() -> Ed25519PrivateKey:
    encoded = os.environ.get(_KEY_ENV)
    if not encoded:
        raise ValueError(f"{_KEY_ENV} is required")
    try:
        raw = base64.b64decode(encoded, validate=True)
    except (binascii.Error, ValueError) as error:
        raise ValueError(f"{_KEY_ENV} must be valid base64") from error
    if len(raw) != 32:
        raise ValueError(f"{_KEY_ENV} must decode to 32 raw bytes")
    return Ed25519PrivateKey.from_private_bytes(raw)


def _write_atomic(path: Path, payload: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_bytes(payload)
    os.replace(temporary, path)


def build_package_plan(
    *, channel: str, version: str, build_id: str, asset_path: Path,
    inventory: list[dict[str, object]], output_dir: Path,
    key_id: str | None = None,
) -> tuple[Path, Path]:
    """Sign the immutable Bootstrap PackagePlan with the canonical key loader.

    The plan is a sidecar release asset rather than a ZIP member: its asset
    digest can therefore bind the already-finalized ZIP without a circular
    self-hash. No second private signer is introduced.
    """
    if channel not in _CHANNELS or not _SEMVER.fullmatch(version):
        raise ValueError("package plan channel/version is invalid")
    asset_path = Path(asset_path)
    if not asset_path.is_file() or asset_path.stat().st_size <= 0:
        raise ValueError("package plan asset must be a nonempty file")
    normalized: list[dict[str, object]] = []
    for item in sorted(inventory, key=lambda value: str(value["relative_path"]).encode("utf-8")):
        relative = str(item["relative_path"])
        payload = Path(str(item["source_path"]))
        if not payload.is_file() or relative in {"", ".", ".."} or relative.replace("\\", "/").startswith("/"):
            raise ValueError("package inventory contains an invalid file")
        normalized.append({
            "byte_size": payload.stat().st_size,
            "relative_path": relative.replace("\\", "/"),
            "sha256": _sha256(payload),
        })
    plan = {
        "asset_sha256": _sha256(asset_path),
        "asset_size": asset_path.stat().st_size,
        "build_id": build_id,
        "channel": channel,
        "entrypoint": "HarkenSearch.exe",
        "inventory": normalized,
        "key_id": key_id or os.environ.get(_PACKAGE_KEY_ID_ENV, "aris-release-v1"),
        "product": "aris",
        "removals": [],
        "schema_version": 1,
        "version": version,
    }
    payload = json.dumps(plan, sort_keys=True, separators=(",", ":")).encode("utf-8")
    signature = _load_private_key().sign(payload).hex().encode("ascii")
    destination = Path(output_dir)
    destination.mkdir(parents=True, exist_ok=True)
    package = destination / "ArisBootstrap.package.json"
    package_signature = destination / "ArisBootstrap.package.json.sig"
    _write_atomic(package, payload)
    _write_atomic(package_signature, signature)
    return package, package_signature


def build_package_plan_with_ephemeral_key(**kwargs: object) -> tuple[Path, Path, bytes]:
    """Local fixture helper; production callers must use build_package_plan."""
    raw = secrets.token_bytes(32)
    private = Ed25519PrivateKey.from_private_bytes(raw)
    public = private.public_key().public_bytes(
        serialization.Encoding.Raw, serialization.PublicFormat.Raw,
    )
    previous = os.environ.get(_KEY_ENV)
    os.environ[_KEY_ENV] = base64.b64encode(raw).decode("ascii")
    try:
        package, signature = build_package_plan(**kwargs)
    finally:
        if previous is None:
            os.environ.pop(_KEY_ENV, None)
        else:
            os.environ[_KEY_ENV] = previous
    return package, signature, public


def promote_package_plan(
    *, testing_plan_bytes: bytes, asset_path: Path, output_dir: Path,
    key_id: str | None = None,
) -> tuple[Path, Path]:
    """Re-sign the immutable testing inventory for the stable channel.

    The ZIP is never rebuilt. Every signed inventory member is re-read from
    the exact ZIP before the stable plan is emitted through the canonical
    private-key loader.
    """
    try:
        source = json.loads(testing_plan_bytes)
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ValueError("testing package plan is invalid JSON") from error
    expected_keys = {
        "asset_sha256", "asset_size", "build_id", "channel", "entrypoint",
        "inventory", "key_id", "product", "removals", "schema_version",
        "version",
    }
    if not isinstance(source, dict) or set(source) != expected_keys:
        raise ValueError("testing package plan schema is not closed")
    asset_path = Path(asset_path)
    if (source.get("schema_version") != 1 or source.get("product") != "aris" or
        source.get("channel") != "testing" or
        source.get("asset_sha256") != _sha256(asset_path) or
        source.get("asset_size") != asset_path.stat().st_size or
        not isinstance(source.get("inventory"), list)):
        raise ValueError("testing package plan does not bind the immutable ZIP")
    seen: set[str] = set()
    with zipfile.ZipFile(asset_path) as archive:
        for item in source["inventory"]:
            if not isinstance(item, dict) or set(item) != {
                "byte_size", "relative_path", "sha256"
            }:
                raise ValueError("testing package inventory is malformed")
            relative = item["relative_path"]
            if not isinstance(relative, str) or relative in seen:
                raise ValueError("testing package inventory is ambiguous")
            seen.add(relative)
            try:
                payload = archive.read(relative)
            except KeyError as error:
                raise ValueError("testing package inventory member is missing") from error
            if (len(payload) != item["byte_size"] or
                hashlib.sha256(payload).hexdigest() != item["sha256"]):
                raise ValueError("testing package inventory member drifted")
    promoted = dict(source)
    promoted["channel"] = "stable"
    promoted["key_id"] = key_id or os.environ.get(
        _PACKAGE_KEY_ID_ENV, "aris-release-v1"
    )
    payload = json.dumps(
        promoted, sort_keys=True, separators=(",", ":")
    ).encode("utf-8")
    signature = _load_private_key().sign(payload).hex().encode("ascii")
    destination = Path(output_dir)
    package = destination / "ArisBootstrap.package.stable.json"
    package_signature = destination / "ArisBootstrap.package.stable.json.sig"
    _write_atomic(package, payload)
    _write_atomic(package_signature, signature)
    return package, package_signature


def build_manifest(
    *,
    channel: str,
    version: str,
    asset_path: Path,
    repository: str,
    build_id: str,
    published_at: str,
    allow_stable: bool,
    minimum_bootstrap_version: str = "1.0.0",
    minimum_direct_version: str = "1.0.0",
    output_dir: Path | None = None,
) -> tuple[Path, Path]:
    if channel not in _CHANNELS:
        raise ValueError("channel must be testing or stable")
    if channel == "stable" and not allow_stable:
        raise ValueError("stable signing requires --allow-stable")
    for label, value in (
        ("version", version),
        ("minimum-bootstrap-version", minimum_bootstrap_version),
        ("minimum-direct-version", minimum_direct_version),
    ):
        if not _SEMVER.fullmatch(value):
            raise ValueError(f"{label} must be strict numeric semver")
    if not _REPOSITORY.fullmatch(repository):
        raise ValueError("repository must be an owner/name GitHub repository")
    if not _PUBLISHED_AT.fullmatch(published_at):
        raise ValueError("published-at must be UTC YYYY-MM-DDTHH:MM:SSZ")
    if not build_id or any(character.isspace() for character in build_id):
        raise ValueError("build-id must be non-empty and contain no whitespace")

    asset_path = Path(asset_path)
    if not asset_path.is_file():
        raise ValueError(f"update asset is not a file: {asset_path}")
    if asset_path.stat().st_size <= 0:
        raise ValueError("update asset must be nonempty")
    expected_name = f"Aris-{version}-win-x64-update.zip"
    if asset_path.name != expected_name:
        raise ValueError(f"update asset must be named {expected_name}")

    key = _load_private_key()
    document = {
        "architecture": "x64",
        "asset": {
            "sha256": _sha256(asset_path),
            "size": asset_path.stat().st_size,
            "url": (
                f"https://github.com/{repository}/releases/download/"
                f"v{version}/{asset_path.name}"
            ),
        },
        "build_id": build_id,
        "channel": channel,
        "data_compatibility": "backward_compatible",
        "minimum_bootstrap_version": minimum_bootstrap_version,
        "minimum_direct_version": minimum_direct_version,
        "platform": "windows",
        "product": "aris",
        "published_at": published_at,
        "schema_version": 1,
        "version": version,
    }
    payload = json.dumps(document, sort_keys=True, separators=(",", ":")).encode("utf-8")
    signature = key.sign(payload).hex().encode("ascii")

    destination = Path(output_dir) if output_dir is not None else asset_path.parent
    manifest_path = destination / manifest_filename(repository, channel)
    signature_path = manifest_path.with_name(manifest_path.name + ".sig")
    _write_atomic(manifest_path, payload)
    _write_atomic(signature_path, signature)
    if repository == PUBLIC_REPOSITORY and channel == "stable":
        # Already-installed clients validate the historical GitHub identity
        # and map it to the company mirror. Keep that identity for resumable
        # downloads; both generations receive these exact same package bytes.
        legacy = dict(document, asset=dict(document["asset"]))
        legacy["asset"]["url"] = (
            f"https://github.com/{LEGACY_UPDATE_REPOSITORY}/releases/download/"
            f"v{version}/{asset_path.name}"
        )
        legacy_payload = json.dumps(legacy, sort_keys=True, separators=(",", ":")).encode("utf-8")
        legacy_path = destination / manifest_filename(LEGACY_UPDATE_REPOSITORY, channel)
        _write_atomic(legacy_path, legacy_payload)
        _write_atomic(legacy_path.with_name(legacy_path.name + ".sig"), key.sign(legacy_payload).hex().encode("ascii"))
    return manifest_path, signature_path


def build_manifest_with_ephemeral_key(**kwargs: object) -> tuple[Path, Path, bytes]:
    """Build a local-only fixture without exposing or persisting its private key."""
    raw = secrets.token_bytes(32)
    private = Ed25519PrivateKey.from_private_bytes(raw)
    public = private.public_key().public_bytes(
        serialization.Encoding.Raw,
        serialization.PublicFormat.Raw,
    )
    previous = os.environ.get(_KEY_ENV)
    os.environ[_KEY_ENV] = base64.b64encode(raw).decode("ascii")
    try:
        manifest, signature = build_manifest(**kwargs)
    finally:
        if previous is None:
            os.environ.pop(_KEY_ENV, None)
        else:
            os.environ[_KEY_ENV] = previous
    return manifest, signature, public


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--channel", choices=sorted(_CHANNELS), required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--asset", type=Path, required=True)
    parser.add_argument("--repository", required=True)
    parser.add_argument("--build-id", required=True)
    parser.add_argument("--published-at", required=True)
    parser.add_argument("--minimum-bootstrap-version", default="1.0.0")
    parser.add_argument("--minimum-direct-version", default="1.0.0")
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--allow-stable", action="store_true")
    return parser


def main() -> int:
    args = _parser().parse_args()
    manifest_path, signature_path = build_manifest(
        channel=args.channel,
        version=args.version,
        asset_path=args.asset,
        repository=args.repository,
        build_id=args.build_id,
        published_at=args.published_at,
        allow_stable=args.allow_stable,
        minimum_bootstrap_version=args.minimum_bootstrap_version,
        minimum_direct_version=args.minimum_direct_version,
        output_dir=args.output_dir,
    )
    print(manifest_path)
    print(signature_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

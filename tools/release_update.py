#!/usr/bin/env python3
"""Build, verify, publish, and promote Aris GitHub update releases.

Private signing is deliberately delegated to ``build_update_manifest.py``.
This module handles deterministic delivery bytes and public verification only.
"""

from __future__ import annotations

import argparse
import base64
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import sys
import tempfile
from typing import Any, Iterable, NamedTuple, Protocol
import zipfile

from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PublicKey

if __package__ in {None, ""}:
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tools.build_update_manifest import (
    build_manifest,
    manifest_filename,
    PUBLIC_REPOSITORY,
    LEGACY_UPDATE_REPOSITORY,
    build_manifest_with_ephemeral_key,
    build_package_plan,
    build_package_plan_with_ephemeral_key,
    promote_package_plan,
)


SEMVER = re.compile(r"(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\Z")
LOWER_HEX_SIGNATURE = re.compile(rb"[0-9a-f]{128}\Z")
SHA256 = re.compile(r"[0-9a-f]{64}\Z")
BOOTSTRAP_NAME = "HarkenSearchBootstrap.exe"
# 旧客户端兼容供给: signed manifest and COS object key stay on this name.
CANONICAL_UPDATE_ZIP_TEMPLATE = "Aris-{version}-win-x64-update.zip"
# Additional published alias for new-client local staging identity.
NEW_CLIENT_UPDATE_ZIP_TEMPLATE = "HarkenSearch-{version}-win-x64-update.zip"
PACKAGE_PLAN_NAME = "ArisBootstrap.package.json"
PACKAGE_SIGNATURE_NAME = "ArisBootstrap.package.json.sig"
STABLE_PACKAGE_PLAN_NAME = "ArisBootstrap.package.stable.json"
STABLE_PACKAGE_SIGNATURE_NAME = "ArisBootstrap.package.stable.json.sig"
CHANNELS = {"testing", "stable"}
MANIFEST_KEYS = {
    "architecture", "asset", "build_id", "channel", "data_compatibility",
    "minimum_bootstrap_version", "minimum_direct_version", "platform",
    "product", "published_at", "schema_version", "version",
}
ASSET_KEYS = {"sha256", "size", "url"}


class ReleaseFixture(NamedTuple):
    bootstrap: Path
    update_zip: Path
    installer: Path
    manifest: Path
    signature: Path


class ReleaseGateway(Protocol):
    def public_get(self, endpoint: str) -> Any: ...
    def release(self, tag: str) -> dict[str, Any] | None: ...
    def create_testing_release(self, tag: str) -> None: ...
    def upload(self, tag: str, name: str, payload: bytes) -> None: ...
    def download(self, tag: str, name: str) -> bytes: ...
    def make_testing_visible(self, tag: str) -> None: ...


def _nonempty_file(path: Path, label: str) -> Path:
    path = Path(path)
    if not path.is_file() or path.is_symlink() or path.stat().st_size <= 0:
        raise ValueError(f"{label} must be one nonempty regular file: {path}")
    return path


def _atomic_copy(source: Path, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_name(destination.name + ".tmp")
    shutil.copyfile(source, temporary)
    os.replace(temporary, destination)


def _payload_entries(payload_dir: Path) -> list[tuple[str, Path]]:
    payload_dir = Path(payload_dir)
    if not payload_dir.is_dir() or payload_dir.is_symlink():
        raise ValueError("payload-dir must be one regular directory")
    entries: list[tuple[str, Path]] = []
    folded: set[str] = set()
    for path in payload_dir.rglob("*"):
        if path.is_symlink():
            raise ValueError(f"payload may not contain symlinks: {path}")
        if path.is_dir():
            continue
        if not path.is_file():
            raise ValueError(f"payload contains a non-regular entry: {path}")
        relative = PurePosixPath(path.relative_to(payload_dir).as_posix())
        if relative.is_absolute() or ".." in relative.parts:
            raise ValueError(f"unsafe payload path: {relative}")
        name = str(relative)
        key = name.casefold()
        if key in folded:
            raise ValueError(f"case-insensitive duplicate payload path: {name}")
        folded.add(key)
        entries.append((name, path))
    if not entries:
        raise ValueError("payload-dir must contain at least one file")
    if BOOTSTRAP_NAME.casefold() in folded:
        raise ValueError("payload-dir must not contain an ambiguous HarkenSearchBootstrap.exe")
    return sorted(entries, key=lambda item: item[0].encode("utf-8"))


def canonical_update_zip_name(version: str) -> str:
    return CANONICAL_UPDATE_ZIP_TEMPLATE.format(version=version)


def new_client_update_zip_name(version: str) -> str:
    return NEW_CLIENT_UPDATE_ZIP_TEMPLATE.format(version=version)


def write_compat_update_zip_alias(update_zip: Path, *, version: str) -> Path:
    """Copy canonical zip bytes to the new-client filename. 旧客户端兼容供给."""
    alias = Path(update_zip).with_name(new_client_update_zip_name(version))
    _atomic_copy(Path(update_zip), alias)
    if alias.read_bytes() != Path(update_zip).read_bytes():
        raise ValueError("HarkenSearch update zip alias bytes drifted from canonical Aris zip")
    return alias


def build_deterministic_zip(payload_dir: Path, bootstrap_path: Path, output_path: Path) -> Path:
    """Write a byte-reproducible update ZIP containing the supplied Bootstrap."""
    bootstrap_path = _nonempty_file(bootstrap_path, "bootstrap")
    if bootstrap_path.name != BOOTSTRAP_NAME:
        raise ValueError(f"bootstrap must be named {BOOTSTRAP_NAME}")
    entries = _payload_entries(payload_dir)
    entries.append((BOOTSTRAP_NAME, bootstrap_path))
    entries.sort(key=lambda item: item[0].encode("utf-8"))
    output_path = Path(output_path)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    temporary = output_path.with_name(output_path.name + ".tmp")
    try:
        with zipfile.ZipFile(temporary, "w") as archive:
            for name, source in entries:
                info = zipfile.ZipInfo(name, (1980, 1, 1, 0, 0, 0))
                info.compress_type = zipfile.ZIP_DEFLATED
                info.create_system = 3
                info.external_attr = 0o100644 << 16
                info.flag_bits |= 0x800
                archive.writestr(info, source.read_bytes(), compresslevel=9)
        if temporary.stat().st_size <= 0:
            raise ValueError("deterministic update ZIP is empty")
        os.replace(temporary, output_path)
    finally:
        if temporary.exists():
            temporary.unlink()
    return output_path


def build_channel_manifest_from_existing_zip(
    update_zip: Path, *, version: str, repository: str, build_id: str,
    published_at: str, channel: str, output_dir: Path,
) -> tuple[Path, Path]:
    return build_manifest(
        channel=channel, version=version, asset_path=Path(update_zip),
        repository=repository, build_id=build_id, published_at=published_at,
        allow_stable=channel == "stable", output_dir=Path(output_dir),
    )


def prepare_release_fixture(
    *, version: str, repository: str, build_id: str, published_at: str,
    payload_dir: Path, bootstrap_path: Path, installer_path: Path,
    channel: str, output_dir: Path,
) -> ReleaseFixture:
    if not SEMVER.fullmatch(version):
        raise ValueError("version must be strict numeric semver")
    if channel not in CHANNELS:
        raise ValueError("channel must be testing or stable")
    bootstrap_source = _nonempty_file(bootstrap_path, "bootstrap")
    installer_source = _nonempty_file(installer_path, "installer")
    if bootstrap_source.name != BOOTSTRAP_NAME:
        raise ValueError(f"bootstrap must be named {BOOTSTRAP_NAME}")
    expected_installer = f"HarkenSearch-{version}-win-x64-setup.exe"
    if installer_source.name != expected_installer:
        raise ValueError(f"installer must be named {expected_installer}")
    output_dir = Path(output_dir)
    bootstrap = output_dir / BOOTSTRAP_NAME
    update_zip = output_dir / canonical_update_zip_name(version)
    installer = output_dir / f"HarkenSearch-{version}-win-x64-setup.exe"
    _atomic_copy(bootstrap_source, bootstrap)
    build_deterministic_zip(payload_dir, bootstrap_source, update_zip)
    write_compat_update_zip_alias(update_zip, version=version)
    inventory = [
        {"relative_path": name, "source_path": path}
        for name, path in _payload_entries(payload_dir)
    ]
    inventory.append(
        {"relative_path": BOOTSTRAP_NAME, "source_path": bootstrap_source}
    )
    build_package_plan(
        channel=channel, version=version, build_id=build_id,
        asset_path=update_zip, inventory=inventory, output_dir=output_dir,
    )
    _atomic_copy(installer_source, installer)
    manifest, signature = build_channel_manifest_from_existing_zip(
        update_zip, version=version, repository=repository, build_id=build_id,
        published_at=published_at, channel=channel, output_dir=output_dir,
    )
    fixture = ReleaseFixture(bootstrap, update_zip, installer, manifest, signature)
    verify_delivery_pair(fixture, version=version)
    return fixture


def verify_delivery_pair(fixture: ReleaseFixture, *, version: str) -> dict[str, Any]:
    """Bind portable ZIP, standalone Bootstrap, and installer bytes for both forms."""
    expected_zip = canonical_update_zip_name(version)
    expected_setup = f"HarkenSearch-{version}-win-x64-setup.exe"
    if fixture.update_zip.name != expected_zip or fixture.installer.name != expected_setup:
        raise ValueError("portable and installed release names are not paired")
    bootstrap = _nonempty_file(fixture.bootstrap, "bootstrap")
    installer = _nonempty_file(fixture.installer, "installer")
    update_zip = _nonempty_file(fixture.update_zip, "update ZIP")
    alias_zip = _nonempty_file(
        fixture.update_zip.with_name(new_client_update_zip_name(version)),
        "HarkenSearch update ZIP alias",
    )
    if alias_zip.read_bytes() != update_zip.read_bytes():
        raise ValueError("HarkenSearch update zip alias bytes drifted from canonical Aris zip")
    _nonempty_file(fixture.update_zip.with_name(PACKAGE_PLAN_NAME), "Bootstrap package plan")
    _nonempty_file(fixture.update_zip.with_name(PACKAGE_SIGNATURE_NAME), "Bootstrap package signature")
    try:
        with zipfile.ZipFile(update_zip) as archive:
            names = archive.namelist()
            if names.count(BOOTSTRAP_NAME) != 1 or archive.read(BOOTSTRAP_NAME) != bootstrap.read_bytes():
                raise ValueError("portable ZIP and standalone Bootstrap bytes are not paired")
    except (zipfile.BadZipFile, KeyError) as error:
        raise ValueError("update ZIP is not a readable portable payload") from error
    return {
        "version": version,
        "bootstrap_sha256": hashlib.sha256(bootstrap.read_bytes()).hexdigest(),
        "update_sha256": hashlib.sha256(update_zip.read_bytes()).hexdigest(),
        "update_size": update_zip.stat().st_size,
        "installer_sha256": hashlib.sha256(installer.read_bytes()).hexdigest(),
        "installer_size": installer.stat().st_size,
    }


def _strict_object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def verify_manifest_bytes(
    manifest: bytes, signature: bytes, asset: bytes, public_key: bytes, *,
    channel: str, version: str, repository: str,
) -> dict[str, Any]:
    if not LOWER_HEX_SIGNATURE.fullmatch(signature):
        raise ValueError("signature must be exactly 64 lowercase-hex bytes")
    if len(public_key) != 32:
        raise ValueError("public key must contain 32 raw bytes")
    try:
        Ed25519PublicKey.from_public_bytes(public_key).verify(bytes.fromhex(signature.decode("ascii")), manifest)
    except (InvalidSignature, ValueError) as error:
        raise ValueError("detached signature does not verify exact manifest bytes") from error
    try:
        document = json.loads(manifest.decode("utf-8"), object_pairs_hook=_strict_object)
    except (UnicodeError, json.JSONDecodeError) as error:
        raise ValueError("manifest must be strict UTF-8 JSON") from error
    if not isinstance(document, dict) or set(document) != MANIFEST_KEYS:
        raise ValueError("manifest schema-v1 fields are not exact")
    if not isinstance(document.get("asset"), dict) or set(document["asset"]) != ASSET_KEYS:
        raise ValueError("manifest schema-v1 must contain exactly one asset object")
    expected_name = canonical_update_zip_name(version)
    expected_url = f"https://github.com/{repository}/releases/download/v{version}/{expected_name}"
    expected = {
        "schema_version": 1, "product": "aris", "channel": channel,
        "version": version, "platform": "windows", "architecture": "x64",
        "data_compatibility": "backward_compatible",
    }
    if any(document.get(key) != value for key, value in expected.items()):
        raise ValueError("manifest identity/channel/version is not bound")
    for key in ("minimum_bootstrap_version", "minimum_direct_version"):
        if not isinstance(document[key], str) or not SEMVER.fullmatch(document[key]):
            raise ValueError(f"manifest {key} is not strict semver")
    if (
        not isinstance(document["build_id"], str)
        or not document["build_id"]
        or any(character.isspace() for character in document["build_id"])
        or not isinstance(document["published_at"], str)
        or not re.fullmatch(r"[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z", document["published_at"])
    ):
        raise ValueError("manifest build identity/timestamp is malformed")
    identity = document["asset"]
    if (
        identity.get("url") != expected_url
        or type(identity.get("size")) is not int
        or identity.get("size") != len(asset)
        or identity.get("size", 0) <= 0
        or identity.get("sha256") != hashlib.sha256(asset).hexdigest()
        or not SHA256.fullmatch(str(identity.get("sha256", "")))
    ):
        raise ValueError("manifest asset name/hash/size/repository/tag is not bound")
    return document


def verify_manifest_bundle(
    manifest_path: Path, signature_path: Path, asset_path: Path, public_key: bytes,
    *, channel: str, version: str, repository: str,
) -> dict[str, Any]:
    return verify_manifest_bytes(
        Path(manifest_path).read_bytes(), Path(signature_path).read_bytes(),
        Path(asset_path).read_bytes(), public_key, channel=channel,
        version=version, repository=repository,
    )


def _remote_names(release: dict[str, Any]) -> set[str]:
    assets = release.get("assets")
    if not isinstance(assets, list):
        raise ValueError("release assets are malformed")
    names = [item.get("name") for item in assets if isinstance(item, dict)]
    if any(not isinstance(name, str) or not name for name in names) or len(names) != len(set(names)):
        raise ValueError("release has missing or duplicate asset names")
    return set(names)


def _manifest_pair(release: dict[str, Any], repository: str, manifest_name: str) -> bool:
    if release.get("draft") is not False or not isinstance(release.get("tag_name"), str):
        return False
    assets = release.get("assets")
    if not isinstance(assets, list):
        return False
    expected = {manifest_name, manifest_name + ".sig"}
    selected = [asset for asset in assets if isinstance(asset, dict) and asset.get("name") in expected]
    if len(selected) != 2 or {asset.get("name") for asset in selected} != expected:
        return False
    prefix = f"https://github.com/{repository}/releases/download/{release['tag_name']}/"
    return all(asset.get("browser_download_url") == prefix + str(asset.get("name")) for asset in selected)


def discover_testing(github: ReleaseGateway, repository: str) -> dict[str, Any]:
    releases = github.public_get(f"/repos/{repository}/releases")
    if not isinstance(releases, list):
        raise ValueError("GitHub Releases list response is malformed")
    candidates = [
        release for release in releases
        if isinstance(release, dict)
        and release.get("prerelease") is True
        and _manifest_pair(release, repository, "manifest-win-x64-testing.json")
    ]
    if not candidates:
        raise ValueError("no exact public testing manifest/signature pair")
    return candidates[0]


def discover_stable(github: ReleaseGateway, repository: str) -> dict[str, Any]:
    release = github.public_get(f"/repos/{repository}/releases/latest")
    if (
        not isinstance(release, dict)
        or release.get("prerelease") is not False
        or not _manifest_pair(release, repository, manifest_filename(repository, "stable"))
    ):
        raise ValueError("latest is not an exact public stable manifest/signature pair")
    return release


def _upload_idempotent(github: ReleaseGateway, tag: str, files: Iterable[Path]) -> None:
    files = tuple(Path(path) for path in files)
    release = github.release(tag)
    if release is None:
        raise ValueError(f"release does not exist: {tag}")
    remote = _remote_names(release)
    for path in files:
        path = _nonempty_file(Path(path), "release asset")
        payload = path.read_bytes()
        if path.name in remote:
            if github.download(tag, path.name) != payload:
                raise ValueError(f"conflicting remote asset: {path.name}")
            continue
        github.upload(tag, path.name, payload)
        remote.add(path.name)
    for path in files:
        path = Path(path)
        if github.download(tag, path.name) != path.read_bytes():
            raise ValueError(f"uploaded asset failed round-trip verification: {path.name}")


def publish_testing(
    github: ReleaseGateway, repository: str, release_tag: str, files: Iterable[Path]
) -> None:
    del repository  # Repository binding is owned by the gateway instance.
    release = github.release(release_tag)
    created = release is None
    if created:
        github.create_testing_release(release_tag)
        release = github.release(release_tag)
    if not isinstance(release, dict) or release.get("prerelease") is not True:
        raise ValueError("testing publication requires a prerelease")
    if not created and release.get("draft") is False:
        _upload_idempotent(github, release_tag, files)
        return
    _upload_idempotent(github, release_tag, files)
    github.make_testing_visible(release_tag)


def _latest(github: ReleaseGateway, repository: str) -> dict[str, Any] | None:
    value = github.public_get(f"/repos/{repository}/releases/latest")
    if value is None:
        return None
    if not isinstance(value, dict):
        raise ValueError("latest release response is malformed")
    return value


def prepare_stable_promotion(
    github: ReleaseGateway, *, repository: str, release_tag: str,
    previous_stable: str | None, update_zip: Path, stable_manifest: Path,
    stable_signature: Path, testing_public_key: bytes, stable_public_key: bytes,
    approved: bool,
) -> None:
    if not approved:
        raise PermissionError("protected stable approval is required")
    if testing_public_key == stable_public_key or len(testing_public_key) != 32 or len(stable_public_key) != 32:
        raise ValueError("testing and stable signing authorities must be separate")
    latest = _latest(github, repository)
    if previous_stable:
        if previous_stable == release_tag:
            raise ValueError("stable predecessor cannot be the candidate itself")
        if latest is None or latest.get("tag_name") != previous_stable:
            raise ValueError("stable predecessor was missing or substituted")
    elif latest is not None:
        raise ValueError("previous_stable is required when a stable release already exists")
    release = github.release(release_tag)
    if not isinstance(release, dict) or release.get("prerelease") is not True or release.get("draft") is not False:
        raise ValueError("stable candidate must be the visible testing prerelease")
    version = release_tag[1:] if release_tag.startswith("v") else ""
    required_names = {
        BOOTSTRAP_NAME,
        canonical_update_zip_name(version),
        new_client_update_zip_name(version),
        f"HarkenSearch-{version}-win-x64-setup.exe",
        PACKAGE_PLAN_NAME,
        PACKAGE_SIGNATURE_NAME,
        "manifest-win-x64-testing.json",
        "manifest-win-x64-testing.json.sig",
    }
    if not required_names.issubset(_remote_names(release)):
        raise ValueError("testing release is missing an approved delivery asset")
    update_zip = _nonempty_file(Path(update_zip), "immutable update ZIP")
    remote_zip = github.download(release_tag, update_zip.name)
    if remote_zip != update_zip.read_bytes():
        raise ValueError("promotion must reuse the exact immutable testing ZIP")
    testing_document = verify_manifest_bytes(
        github.download(release_tag, "manifest-win-x64-testing.json"),
        github.download(release_tag, "manifest-win-x64-testing.json.sig"),
        remote_zip, testing_public_key, channel="testing", version=version,
        repository=repository,
    )
    testing_package = github.download(release_tag, PACKAGE_PLAN_NAME)
    testing_package_signature = github.download(
        release_tag, PACKAGE_SIGNATURE_NAME
    )
    if not LOWER_HEX_SIGNATURE.fullmatch(testing_package_signature):
        raise ValueError("testing package signature encoding is invalid")
    try:
        Ed25519PublicKey.from_public_bytes(testing_public_key).verify(
            bytes.fromhex(testing_package_signature.decode("ascii")),
            testing_package,
        )
    except (InvalidSignature, ValueError) as error:
        raise ValueError("testing package signature is invalid") from error
    stable_package, stable_package_signature = promote_package_plan(
        testing_plan_bytes=testing_package,
        asset_path=update_zip,
        output_dir=update_zip.parent,
    )
    if (stable_package.name != STABLE_PACKAGE_PLAN_NAME or
        stable_package_signature.name != STABLE_PACKAGE_SIGNATURE_NAME):
        raise ValueError("stable package sidecar naming drifted")
    stable_document = verify_manifest_bundle(
        stable_manifest, stable_signature, update_zip, stable_public_key,
        channel="stable", version=version, repository=repository,
    )
    for key in ("version", "build_id", "data_compatibility", "minimum_bootstrap_version", "minimum_direct_version"):
        if testing_document[key] != stable_document[key]:
            raise ValueError(f"testing/stable manifest drift: {key}")
    stable_files = [stable_package, stable_package_signature, stable_manifest, stable_signature]
    if repository == PUBLIC_REPOSITORY:
        legacy_manifest = Path(stable_manifest).with_name(manifest_filename(LEGACY_UPDATE_REPOSITORY, "stable"))
        legacy_signature = legacy_manifest.with_name(legacy_manifest.name + ".sig")
        legacy_document = verify_manifest_bundle(
            legacy_manifest, legacy_signature, update_zip, stable_public_key,
            channel="stable", version=version, repository=LEGACY_UPDATE_REPOSITORY,
        )
        legacy_document["asset"]["url"] = stable_document["asset"]["url"]
        if legacy_document != stable_document:
            raise ValueError("legacy/new stable manifest identity drift")
        stable_files.extend([legacy_manifest, legacy_signature])
    _upload_idempotent(
        github, release_tag, stable_files,
    )
    reread = _latest(github, repository)
    if previous_stable:
        if (reread is None or reread.get("tag_name") != previous_stable or
                reread.get("id") != latest.get("id")):
            raise ValueError("stable predecessor changed before final visibility")
    elif reread is not None:
        raise ValueError("a stable release appeared before first stable visibility")


def prepare_rollback(
    github: ReleaseGateway, *, repository: str, current_tag: str,
    previous_stable: str | None, expected_previous_id: int, approved: bool,
) -> None:
    if not approved:
        raise PermissionError("protected rollback approval is required")
    if not previous_stable:
        raise ValueError("previous_stable=null cannot be rolled back")
    if previous_stable == current_tag:
        raise ValueError("rollback predecessor cannot be current stable")
    latest = _latest(github, repository)
    if latest is None or latest.get("tag_name") != current_tag or not _manifest_pair(latest, repository, manifest_filename(repository, "stable")):
        raise ValueError("current stable changed before rollback")
    predecessor = github.release(previous_stable)
    if (
        not isinstance(predecessor, dict)
        or predecessor.get("id") != expected_previous_id
        or predecessor.get("tag_name") != previous_stable
        or predecessor.get("draft") is not False
        or predecessor.get("prerelease") is not False
        or not _manifest_pair(predecessor, repository, manifest_filename(repository, "stable"))
    ):
        raise ValueError("rollback predecessor is missing or substituted")


def local_dry_run(**kwargs: Any) -> dict[str, str]:
    version = kwargs["version"]
    output_dir = Path(kwargs["output_dir"])
    bootstrap = output_dir / BOOTSTRAP_NAME
    update_zip = output_dir / canonical_update_zip_name(version)
    installer = output_dir / f"HarkenSearch-{version}-win-x64-setup.exe"
    bootstrap_source = _nonempty_file(Path(kwargs["bootstrap_path"]), "bootstrap")
    installer_source = _nonempty_file(Path(kwargs["installer_path"]), "installer")
    if bootstrap_source.name != BOOTSTRAP_NAME or installer_source.name != f"HarkenSearch-{version}-win-x64-setup.exe":
        raise ValueError("dry-run inputs do not use approved delivery names")
    _atomic_copy(bootstrap_source, bootstrap)
    build_deterministic_zip(Path(kwargs["payload_dir"]), bootstrap_source, update_zip)
    write_compat_update_zip_alias(update_zip, version=version)
    inventory = [
        {"relative_path": name, "source_path": path}
        for name, path in _payload_entries(Path(kwargs["payload_dir"]))
    ]
    build_package_plan_with_ephemeral_key(
        channel="testing", version=version, build_id=kwargs["build_id"],
        asset_path=update_zip, inventory=inventory, output_dir=output_dir,
    )
    _atomic_copy(installer_source, installer)
    manifest, signature, public = build_manifest_with_ephemeral_key(
        channel="testing", version=version, asset_path=update_zip,
        repository=kwargs["repository"], build_id=kwargs["build_id"],
        published_at=kwargs["published_at"], allow_stable=False,
        output_dir=output_dir,
    )
    fixture = ReleaseFixture(bootstrap, update_zip, installer, manifest, signature)
    verify_delivery_pair(fixture, version=version)
    document = verify_manifest_bundle(
        fixture.manifest, fixture.signature, fixture.update_zip, public,
        channel="testing", version=kwargs["version"], repository=kwargs["repository"],
    )
    return {
        "manifest": str(fixture.manifest),
        "manifest_sha256": hashlib.sha256(fixture.manifest.read_bytes()).hexdigest(),
        "public_key_b64": base64.b64encode(public).decode("ascii"),
        "update_sha256": document["asset"]["sha256"],
    }


class GhGateway:
    """Thin real GitHub boundary; all mutation is explicit and no clobber exists."""

    def __init__(self, repository: str) -> None:
        self.repository = repository

    def _gh(self, *args: str) -> str:
        result = subprocess.run(
            ["gh", *args], check=True, text=True, encoding="utf-8",
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        )
        return result.stdout

    def public_get(self, endpoint: str) -> Any:
        result = subprocess.run(
            ["gh", "api", "-H", "Accept: application/vnd.github+json", endpoint],
            check=False, text=True, encoding="utf-8",
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        )
        if result.returncode != 0:
            if "404" in result.stderr and endpoint.endswith("/releases/latest"):
                return None
            raise RuntimeError(f"gh api {endpoint} failed: {result.stderr.strip()}")
        return json.loads(result.stdout)

    def release(self, tag: str) -> dict[str, Any] | None:
        result = subprocess.run(
            ["gh", "release", "view", tag, "--repo", self.repository, "--json", "databaseId,tagName,isDraft,isPrerelease,assets"],
            check=False, text=True, encoding="utf-8", stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        )
        if result.returncode:
            if "not found" in result.stderr.lower():
                return None
            raise subprocess.CalledProcessError(result.returncode, result.args, result.stdout, result.stderr)
        raw = result.stdout
        value = json.loads(raw)
        return {
            "id": value["databaseId"], "tag_name": value["tagName"],
            "draft": value["isDraft"], "prerelease": value["isPrerelease"],
            "assets": [
                {
                    "name": item["name"],
                    "size": item["size"],
                    "browser_download_url": f"https://github.com/{self.repository}/releases/download/{tag}/{item['name']}",
                }
                for item in value["assets"]
            ],
        }

    def create_testing_release(self, tag: str) -> None:
        self._gh("release", "create", tag, "--repo", self.repository, "--draft", "--prerelease", "--title", f"Aris {tag}")

    def upload(self, tag: str, name: str, payload: bytes) -> None:
        with tempfile.TemporaryDirectory(prefix="aris-release-upload-") as directory:
            path = Path(directory) / name
            path.write_bytes(payload)
            self._gh("release", "upload", tag, str(path), "--repo", self.repository)

    def download(self, tag: str, name: str) -> bytes:
        with tempfile.TemporaryDirectory(prefix="aris-release-download-") as directory:
            self._gh("release", "download", tag, "--repo", self.repository, "--pattern", name, "--dir", directory)
            matches = list(Path(directory).glob(name))
            if len(matches) != 1:
                raise ValueError(f"remote release asset is missing or ambiguous: {name}")
            return matches[0].read_bytes()

    def make_testing_visible(self, tag: str) -> None:
        self._gh("release", "edit", tag, "--repo", self.repository, "--draft=false", "--prerelease=true")


def _common_parser(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--version", required=True)
    parser.add_argument("--repository", default=PUBLIC_REPOSITORY)
    parser.add_argument("--build-id", required=True)
    parser.add_argument("--published-at", required=True)
    parser.add_argument("--payload-dir", type=Path, required=True)
    parser.add_argument("--bootstrap", type=Path, required=True)
    parser.add_argument("--installer", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    for name in ("dry-run", "prepare-testing"):
        _common_parser(commands.add_parser(name))
    publish = commands.add_parser("publish-testing")
    publish.add_argument("--repository", default=PUBLIC_REPOSITORY)
    publish.add_argument("--release-tag", required=True)
    publish.add_argument("--asset", action="append", type=Path, required=True)
    verify = commands.add_parser("verify")
    verify.add_argument("--manifest", type=Path, required=True)
    verify.add_argument("--signature", type=Path, required=True)
    verify.add_argument("--asset", type=Path, required=True)
    verify.add_argument("--channel", choices=sorted(CHANNELS), required=True)
    verify.add_argument("--version", required=True)
    verify.add_argument("--repository", default=PUBLIC_REPOSITORY)
    discover = commands.add_parser("discover")
    discover.add_argument("--channel", choices=sorted(CHANNELS), required=True)
    discover.add_argument("--repository", default=PUBLIC_REPOSITORY)
    stable = commands.add_parser("prepare-stable")
    stable.add_argument("--repository", default=PUBLIC_REPOSITORY)
    stable.add_argument("--release-tag", required=True)
    stable.add_argument("--previous-stable", default="")
    stable.add_argument("--update-zip", type=Path, required=True)
    stable.add_argument("--manifest", type=Path, required=True)
    stable.add_argument("--signature", type=Path, required=True)
    stable.add_argument("--approved", action="store_true")
    rollback = commands.add_parser("rollback-check")
    rollback.add_argument("--repository", default=PUBLIC_REPOSITORY)
    rollback.add_argument("--current-tag", required=True)
    rollback.add_argument("--previous-stable", required=True)
    rollback.add_argument("--expected-previous-id", type=int, required=True)
    rollback.add_argument("--approved", action="store_true")
    return parser


def _public_key_from_env(name: str) -> bytes:
    encoded = os.environ.get(name)
    if not encoded:
        raise ValueError(f"{name} is required")
    try:
        raw = base64.b64decode(encoded, validate=True)
    except ValueError as error:
        raise ValueError(f"{name} must be valid base64") from error
    if len(raw) != 32:
        raise ValueError(f"{name} must decode to 32 raw bytes")
    return raw


def main() -> int:
    args = _parser().parse_args()
    if args.command in {"dry-run", "prepare-testing"}:
        values = dict(
            version=args.version, repository=args.repository, build_id=args.build_id,
            published_at=args.published_at, payload_dir=args.payload_dir,
            bootstrap_path=args.bootstrap, installer_path=args.installer,
            output_dir=args.output_dir,
        )
        if args.command == "dry-run":
            print(json.dumps(local_dry_run(**values), sort_keys=True))
        else:
            prepare_release_fixture(channel="testing", **values)
        return 0
    gateway = GhGateway(args.repository)
    if args.command == "publish-testing":
        publish_testing(gateway, args.repository, args.release_tag, args.asset)
    elif args.command == "verify":
        key = _public_key_from_env(f"ARIS_{args.channel.upper()}_UPDATE_PUBLIC_KEY_B64")
        verify_manifest_bundle(
            args.manifest, args.signature, args.asset, key, channel=args.channel,
            version=args.version, repository=args.repository,
        )
    elif args.command == "discover":
        release = discover_testing(gateway, args.repository) if args.channel == "testing" else discover_stable(gateway, args.repository)
        print(json.dumps({"id": release.get("id"), "tag_name": release.get("tag_name")}, sort_keys=True))
    elif args.command == "prepare-stable":
        prepare_stable_promotion(
            gateway, repository=args.repository, release_tag=args.release_tag,
            previous_stable=args.previous_stable, update_zip=args.update_zip,
            stable_manifest=args.manifest, stable_signature=args.signature,
            testing_public_key=_public_key_from_env("ARIS_TESTING_UPDATE_PUBLIC_KEY_B64"),
            stable_public_key=_public_key_from_env("ARIS_STABLE_UPDATE_PUBLIC_KEY_B64"),
            approved=args.approved,
        )
    elif args.command == "rollback-check":
        prepare_rollback(
            gateway, repository=args.repository, current_tag=args.current_tag,
            previous_stable=args.previous_stable,
            expected_previous_id=args.expected_previous_id, approved=args.approved,
        )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ValueError, PermissionError, subprocess.CalledProcessError) as error:
        print(f"release update refused: {error}", file=sys.stderr)
        raise SystemExit(2)

"""Offline behavior contract for the GitHub Releases update pipeline."""

from __future__ import annotations

import base64
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import zipfile

import pytest
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric.ed25519 import (
    Ed25519PrivateKey,
    Ed25519PublicKey,
)


ROOT = Path(__file__).resolve().parents[2]
IMPLEMENTATION = ROOT / "tools" / "release_update.py"


def _load_release_update():
    assert IMPLEMENTATION.is_file(), "missing approved semantic capability: release coordinator"
    spec = importlib.util.spec_from_file_location("release_update", IMPLEMENTATION)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _key(seed: int) -> tuple[bytes, bytes]:
    private = Ed25519PrivateKey.from_private_bytes(bytes([seed]) * 32)
    public = private.public_key().public_bytes(
        serialization.Encoding.Raw, serialization.PublicFormat.Raw
    )
    return bytes([seed]) * 32, public


def _inputs(tmp_path: Path) -> tuple[Path, Path, Path]:
    payload = tmp_path / "payload"
    payload.mkdir(parents=True)
    (payload / "HarkenSearch.exe").write_bytes(b"aris-binary")
    (payload / "resources").mkdir()
    (payload / "resources" / "theme.json").write_bytes(b"{}\n")
    bootstrap = tmp_path / "HarkenSearchBootstrap.exe"
    bootstrap.write_bytes(b"bootstrap-binary")
    setup = tmp_path / "HarkenSearch-1.2.3-win-x64-setup.exe"
    setup.write_bytes(b"setup-binary")
    return payload, bootstrap, setup


def _prepare(module, tmp_path: Path, monkeypatch: pytest.MonkeyPatch, *, seed: int = 1, repository: str = "xindong/aris"):
    payload, bootstrap, setup = _inputs(tmp_path)
    private, public = _key(seed)
    monkeypatch.setenv("ARIS_UPDATE_SIGNING_KEY_B64", base64.b64encode(private).decode("ascii"))
    output = tmp_path / "out"
    result = module.prepare_release_fixture(
        version="1.2.3",
        repository=repository,
        build_id="7167e34",
        published_at="2026-07-29T00:00:00Z",
        payload_dir=payload,
        bootstrap_path=bootstrap,
        installer_path=setup,
        channel="testing",
        output_dir=output,
    )
    return result, public


def test_deterministic_package_has_approved_names_bytes_and_bootstrap(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    module = _load_release_update()
    first, public = _prepare(module, tmp_path / "a", monkeypatch)
    second, _ = _prepare(module, tmp_path / "b", monkeypatch)
    assert first.update_zip.read_bytes() == second.update_zip.read_bytes()
    assert first.update_zip.name == "Aris-1.2.3-win-x64-update.zip"
    alias = first.update_zip.with_name("HarkenSearch-1.2.3-win-x64-update.zip")
    assert alias.is_file() and alias.read_bytes() == first.update_zip.read_bytes()
    assert first.installer.name == "HarkenSearch-1.2.3-win-x64-setup.exe"
    assert first.bootstrap.name == "HarkenSearchBootstrap.exe"
    with zipfile.ZipFile(first.update_zip) as archive:
        assert archive.namelist() == ["HarkenSearch.exe", "HarkenSearchBootstrap.exe", "resources/theme.json"]
        assert archive.read("HarkenSearchBootstrap.exe") == b"bootstrap-binary"
        assert {entry.date_time for entry in archive.infolist()} == {(1980, 1, 1, 0, 0, 0)}
    identity = module.verify_delivery_pair(first, version="1.2.3")
    assert identity["update_size"] == first.update_zip.stat().st_size
    assert identity["installer_size"] == first.installer.stat().st_size > 0
    verified = module.verify_manifest_bundle(
        first.manifest, first.signature, first.update_zip, public,
        channel="testing", version="1.2.3", repository="xindong/aris",
    )
    assert verified["build_id"] == "7167e34"
    signature = first.signature.read_text(encoding="ascii")
    assert len(signature) == 128 and signature == signature.lower()


def test_signed_inventory_authorizes_the_bootstrap_replacement(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    module = _load_release_update()
    fixture, _ = _prepare(module, tmp_path, monkeypatch)
    package = json.loads(
        fixture.update_zip.with_name("ArisBootstrap.package.json").read_text(
            encoding="utf-8"
        )
    )
    inventory = {
        item["relative_path"]: item
        for item in package["inventory"]
    }
    bootstrap = fixture.bootstrap.read_bytes()
    assert inventory["HarkenSearchBootstrap.exe"] == {
        "byte_size": len(bootstrap),
        "relative_path": "HarkenSearchBootstrap.exe",
        "sha256": hashlib.sha256(bootstrap).hexdigest(),
    }


@pytest.mark.parametrize("fault", ["missing_bootstrap", "duplicate_bootstrap", "wrong_bootstrap_name", "wrong_setup_name", "empty_payload", "empty_setup"])
def test_packaging_fails_closed_on_ambiguous_or_empty_inputs(tmp_path: Path, monkeypatch: pytest.MonkeyPatch, fault: str) -> None:
    module = _load_release_update()
    payload, bootstrap, setup = _inputs(tmp_path)
    if fault == "missing_bootstrap":
        bootstrap.unlink()
    elif fault == "duplicate_bootstrap":
        (payload / "HarkenSearchBootstrap.exe").write_bytes(b"other")
    elif fault == "wrong_bootstrap_name":
        wrong = tmp_path / "bootstrap.exe"
        bootstrap.rename(wrong)
        bootstrap = wrong
    elif fault == "wrong_setup_name":
        wrong = tmp_path / "setup.exe"
        setup.rename(wrong)
        setup = wrong
    elif fault == "empty_payload":
        for path in sorted(payload.rglob("*"), reverse=True):
            path.unlink() if path.is_file() else path.rmdir()
    elif fault == "empty_setup":
        setup.write_bytes(b"")
    private, _ = _key(1)
    monkeypatch.setenv("ARIS_UPDATE_SIGNING_KEY_B64", base64.b64encode(private).decode("ascii"))
    with pytest.raises(ValueError):
        module.prepare_release_fixture(
            version="1.2.3", repository="xindong/aris", build_id="build",
            published_at="2026-07-29T00:00:00Z", payload_dir=payload,
            bootstrap_path=bootstrap, installer_path=setup, channel="testing",
            output_dir=tmp_path / "out",
        )


def test_canonical_builder_refuses_a_zero_byte_update_zip(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    module = _load_release_update()
    empty = tmp_path / "Aris-1.2.3-win-x64-update.zip"
    empty.write_bytes(b"")
    private, _ = _key(1)
    monkeypatch.setenv("ARIS_UPDATE_SIGNING_KEY_B64", base64.b64encode(private).decode("ascii"))
    with pytest.raises(ValueError, match="nonempty"):
        module.build_channel_manifest_from_existing_zip(
            empty, version="1.2.3", repository="xindong/aris", build_id="build",
            published_at="2026-07-29T00:00:00Z", channel="testing",
            output_dir=tmp_path,
        )


@pytest.mark.parametrize(
    "fault",
    ["wrong_name", "wrong_hash", "wrong_size", "wrong_signature", "wrong_key", "wrong_channel", "wrong_version", "extra_asset", "uppercase_signature"],
)
def test_manifest_verification_rejects_identity_signature_and_schema_drift(tmp_path: Path, monkeypatch: pytest.MonkeyPatch, fault: str) -> None:
    module = _load_release_update()
    result, public = _prepare(module, tmp_path, monkeypatch)
    manifest = result.manifest.read_bytes()
    signature = result.signature.read_bytes()
    asset = result.update_zip
    channel = "testing"
    version = "1.2.3"
    if fault in {"wrong_name", "wrong_hash", "wrong_size", "extra_asset"}:
        document = json.loads(manifest)
        if fault == "wrong_name":
            document["asset"]["url"] = document["asset"]["url"].replace("update.zip", "other.zip")
        elif fault == "wrong_hash":
            document["asset"]["sha256"] = "0" * 64
        elif fault == "wrong_size":
            document["asset"]["size"] += 1
        else:
            document["assets"] = [document["asset"]]
        manifest = json.dumps(document, sort_keys=True, separators=(",", ":")).encode()
    elif fault == "wrong_signature":
        signature = b"0" * 128
    elif fault == "wrong_key":
        _, public = _key(2)
    elif fault == "wrong_channel":
        channel = "stable"
    elif fault == "wrong_version":
        version = "1.2.4"
    else:
        signature = signature.upper()
    with pytest.raises(ValueError):
        module.verify_manifest_bytes(
            manifest, signature, asset.read_bytes(), public,
            channel=channel, version=version, repository="xindong/aris",
        )


class FakeGitHub:
    def __init__(self, releases: list[dict], latest: dict | None = None) -> None:
        self.releases = releases
        self.latest = latest
        self.assets: dict[tuple[str, str], bytes] = {}
        self.commands: list[tuple[str, ...]] = []
        self.fail_upload_at: int | None = None

    def public_get(self, endpoint: str):
        self.commands.append(("GET", endpoint))
        return self.latest if endpoint.endswith("/latest") else self.releases

    def release(self, tag: str):
        return next((item for item in self.releases if item["tag_name"] == tag), None)

    def create_testing_release(self, tag: str) -> None:
        self.commands.append(("create", tag))
        self.releases.insert(0, {"id": len(self.releases) + 1, "tag_name": tag, "draft": True, "prerelease": True, "assets": []})

    def upload(self, tag: str, name: str, payload: bytes) -> None:
        self.commands.append(("upload", tag, name))
        if self.fail_upload_at is not None and sum(c[0] == "upload" for c in self.commands) == self.fail_upload_at:
            raise RuntimeError("injected partial upload")
        key = (tag, name)
        if key in self.assets and self.assets[key] != payload:
            raise ValueError("conflicting remote asset")
        if key not in self.assets:
            self.assets[key] = payload
            release = self.release(tag)
            release["assets"].append({"name": name, "size": len(payload), "browser_download_url": f"https://github.com/xindong/aris/releases/download/{tag}/{name}"})

    def download(self, tag: str, name: str) -> bytes:
        return self.assets[(tag, name)]

    def make_testing_visible(self, tag: str) -> None:
        self.commands.append(("testing-visible", tag))
        self.release(tag)["draft"] = False

    def make_stable_latest(self, tag: str) -> None:
        self.commands.append(("stable-latest", tag))
        release = self.release(tag)
        release["draft"] = False
        release["prerelease"] = False
        self.latest = release


def _release_files(result) -> list[Path]:
    return [
        result.bootstrap,
        result.update_zip,
        result.update_zip.with_name("HarkenSearch-1.2.3-win-x64-update.zip"),
        result.update_zip.with_name("ArisBootstrap.package.json"),
        result.update_zip.with_name("ArisBootstrap.package.json.sig"),
        result.installer,
        result.manifest,
        result.signature,
    ]


def test_testing_publish_is_idempotent_and_partial_failure_never_changes_visibility(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    module = _load_release_update()
    result, _ = _prepare(module, tmp_path, monkeypatch)
    github = FakeGitHub([])
    github.fail_upload_at = 3
    with pytest.raises(RuntimeError):
        module.publish_testing(github, "xindong/aris", "v1.2.3", _release_files(result))
    assert github.release("v1.2.3")["draft"] is True
    github.fail_upload_at = None
    module.publish_testing(github, "xindong/aris", "v1.2.3", _release_files(result))
    snapshot = dict(github.assets)
    module.publish_testing(github, "xindong/aris", "v1.2.3", _release_files(result))
    assert github.assets == snapshot
    assert sum(command[0] == "testing-visible" for command in github.commands) == 1
    github.assets[("v1.2.3", result.update_zip.name)] = b"substituted"
    with pytest.raises(ValueError, match="conflicting"):
        module.publish_testing(github, "xindong/aris", "v1.2.3", _release_files(result))


def test_public_channel_discovery_uses_list_and_latest_with_exact_bound_pairs() -> None:
    module = _load_release_update()
    testing = {
        "id": 2, "tag_name": "v1.2.3", "draft": False, "prerelease": True,
        "assets": [
            {"name": name, "browser_download_url": f"https://github.com/xindong/aris/releases/download/v1.2.3/{name}"}
            for name in ("manifest-win-x64-testing.json", "manifest-win-x64-testing.json.sig")
        ],
    }
    stable = {
        "id": 1, "tag_name": "v1.2.2", "draft": False, "prerelease": False,
        "assets": [
            {"name": name, "browser_download_url": f"https://github.com/xindong/aris/releases/download/v1.2.2/{name}"}
            for name in ("manifest-win-x64-stable.json", "manifest-win-x64-stable.json.sig")
        ],
    }
    github = FakeGitHub([testing, stable], latest=stable)
    assert module.discover_testing(github, "xindong/aris")["tag_name"] == "v1.2.3"
    assert module.discover_stable(github, "xindong/aris")["tag_name"] == "v1.2.2"
    assert ("GET", "/repos/xindong/aris/releases") in github.commands
    assert ("GET", "/repos/xindong/aris/releases/latest") in github.commands
    testing["assets"][0]["browser_download_url"] = "https://github.com/other/repo/releases/download/v1.2.3/manifest-win-x64-testing.json"
    with pytest.raises(ValueError, match="exact public testing"):
        module.discover_testing(github, "xindong/aris")


def test_promotion_reuses_zip_requires_separate_keys_and_preserves_previous_stable(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    module = _load_release_update()
    testing, testing_public = _prepare(module, tmp_path / "testing", monkeypatch, seed=1)
    github = FakeGitHub([])
    module.publish_testing(github, "xindong/aris", "v1.2.3", _release_files(testing))
    previous = {"id": 7, "tag_name": "v1.2.2", "draft": False, "prerelease": False, "assets": [
        {"name": name, "browser_download_url": f"https://github.com/xindong/aris/releases/download/v1.2.2/{name}"}
        for name in ("manifest-win-x64-stable.json", "manifest-win-x64-stable.json.sig")
    ]}
    github.releases.append(previous)
    github.latest = previous
    private, stable_public = _key(2)
    monkeypatch.setenv("ARIS_UPDATE_SIGNING_KEY_B64", base64.b64encode(private).decode("ascii"))
    stable = module.build_channel_manifest_from_existing_zip(
        testing.update_zip, version="1.2.3", repository="xindong/aris",
        build_id="7167e34", published_at="2026-07-29T00:00:00Z",
        channel="stable", output_dir=tmp_path / "stable",
    )
    github.fail_upload_at = 9
    with pytest.raises(RuntimeError, match="partial upload"):
        module.prepare_stable_promotion(
            github, repository="xindong/aris", release_tag="v1.2.3",
            previous_stable="v1.2.2", update_zip=testing.update_zip,
            stable_manifest=stable[0], stable_signature=stable[1],
            testing_public_key=testing_public, stable_public_key=stable_public,
            approved=True,
        )
    assert github.latest is previous
    github.fail_upload_at = None
    module.prepare_stable_promotion(
        github, repository="xindong/aris", release_tag="v1.2.3",
        previous_stable="v1.2.2", update_zip=testing.update_zip,
        stable_manifest=stable[0], stable_signature=stable[1],
        testing_public_key=testing_public, stable_public_key=stable_public,
        approved=True,
    )
    assert github.latest is previous
    stable_package = github.assets[("v1.2.3", "ArisBootstrap.package.stable.json")]
    stable_package_signature = github.assets[
        ("v1.2.3", "ArisBootstrap.package.stable.json.sig")
    ]
    Ed25519PublicKey.from_public_bytes(stable_public).verify(
        bytes.fromhex(stable_package_signature.decode("ascii")), stable_package
    )
    assert json.loads(stable_package)["channel"] == "stable"
    github.assets[("v1.2.3", stable[0].name)] = b"conflicting-stable-manifest"
    with pytest.raises(ValueError, match="conflicting"):
        module.prepare_stable_promotion(
            github, repository="xindong/aris", release_tag="v1.2.3",
            previous_stable="v1.2.2", update_zip=testing.update_zip,
            stable_manifest=stable[0], stable_signature=stable[1],
            testing_public_key=testing_public, stable_public_key=stable_public,
            approved=True,
        )
    github.assets[("v1.2.3", stable[0].name)] = stable[0].read_bytes()
    github.make_stable_latest("v1.2.3")
    assert github.latest["tag_name"] == "v1.2.3"
    with pytest.raises(ValueError, match="separate"):
        module.prepare_stable_promotion(
            github, repository="xindong/aris", release_tag="v1.2.3",
            previous_stable="v1.2.2", update_zip=testing.update_zip,
            stable_manifest=stable[0], stable_signature=stable[1],
            testing_public_key=stable_public, stable_public_key=stable_public,
            approved=True,
        )


@pytest.mark.parametrize("fault", [None, "bad_signature", "identity_drift", "missing"])
def test_new_repository_promotion_requires_both_signed_manifest_pairs(tmp_path, monkeypatch, fault):
    module = _load_release_update()
    repository = "xindong/harken-search"
    testing, testing_public = _prepare(module, tmp_path / "testing", monkeypatch, repository=repository)
    github = FakeGitHub([])
    module.publish_testing(github, repository, "v1.2.3", _release_files(testing))
    private, stable_public = _key(2)
    monkeypatch.setenv("ARIS_UPDATE_SIGNING_KEY_B64", base64.b64encode(private).decode("ascii"))
    stable = module.build_channel_manifest_from_existing_zip(
        testing.update_zip, version="1.2.3", repository=repository,
        build_id="7167e34", published_at="2026-07-29T00:00:00Z",
        channel="stable", output_dir=tmp_path / "stable",
    )
    legacy = stable[0].with_name("manifest-win-x64-stable.json")
    legacy_signature = legacy.with_name(legacy.name + ".sig")
    if fault == "bad_signature":
        legacy_signature.write_bytes(b"0" * 128)
    elif fault == "identity_drift":
        document = json.loads(legacy.read_bytes())
        document["build_id"] = "another-build"
        legacy.write_bytes(json.dumps(document, sort_keys=True, separators=(",", ":")).encode())
        legacy_signature.write_text(Ed25519PrivateKey.from_private_bytes(private).sign(legacy.read_bytes()).hex(), encoding="ascii")
    elif fault == "missing":
        legacy.unlink()

    def promote():
        module.prepare_stable_promotion(
            github, repository=repository, release_tag="v1.2.3", previous_stable=None,
            update_zip=testing.update_zip, stable_manifest=stable[0], stable_signature=stable[1],
            testing_public_key=testing_public, stable_public_key=stable_public, approved=True,
        )

    before = dict(github.assets)
    if fault:
        with pytest.raises((ValueError, FileNotFoundError)):
            promote()
        assert github.assets == before
        return
    promote()
    for path in (*stable, legacy, legacy_signature):
        assert github.assets[("v1.2.3", path.name)] == path.read_bytes()
    snapshot = dict(github.assets)
    promote()
    assert github.assets == snapshot
    assert github.latest is None


def test_first_stable_is_allowed_only_when_github_has_no_existing_stable(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    module = _load_release_update()
    testing, testing_public = _prepare(module, tmp_path / "testing", monkeypatch, seed=1)
    github = FakeGitHub([])
    module.publish_testing(github, "xindong/aris", "v1.2.3", _release_files(testing))
    private, stable_public = _key(2)
    monkeypatch.setenv(
        "ARIS_UPDATE_SIGNING_KEY_B64", base64.b64encode(private).decode("ascii")
    )
    stable = module.build_channel_manifest_from_existing_zip(
        testing.update_zip, version="1.2.3", repository="xindong/aris",
        build_id="7167e34", published_at="2026-07-29T00:00:00Z",
        channel="stable", output_dir=testing.update_zip.parent,
    )

    module.prepare_stable_promotion(
        github, repository="xindong/aris", release_tag="v1.2.3",
        previous_stable=None, update_zip=testing.update_zip,
        stable_manifest=stable[0], stable_signature=stable[1],
        testing_public_key=testing_public, stable_public_key=stable_public,
        approved=True,
    )
    assert github.latest is None

    existing = {
        "id": 9, "tag_name": "v1.2.2", "draft": False, "prerelease": False,
        "assets": [],
    }
    github.latest = existing
    github.releases.append(existing)
    with pytest.raises(ValueError, match="previous_stable"):
        module.prepare_stable_promotion(
            github, repository="xindong/aris", release_tag="v1.2.3",
            previous_stable=None, update_zip=testing.update_zip,
            stable_manifest=stable[0], stable_signature=stable[1],
            testing_public_key=testing_public, stable_public_key=stable_public,
            approved=True,
        )


@pytest.mark.parametrize("predecessor", [None, "v2.0.0", "v0.9.0"])
def test_rollback_refuses_null_self_and_substitution_before_mutation(predecessor: str | None) -> None:
    module = _load_release_update()
    current = {"id": 2, "tag_name": "v2.0.0", "draft": False, "prerelease": False, "assets": [
        {"name": name, "browser_download_url": f"https://github.com/xindong/aris/releases/download/v2.0.0/{name}"}
        for name in ("manifest-win-x64-stable.json", "manifest-win-x64-stable.json.sig")
    ]}
    old = {"id": 1, "tag_name": "v1.9.0", "draft": False, "prerelease": False, "assets": [
        {"name": name, "browser_download_url": f"https://github.com/xindong/aris/releases/download/v1.9.0/{name}"}
        for name in ("manifest-win-x64-stable.json", "manifest-win-x64-stable.json.sig")
    ]}
    github = FakeGitHub([current, old], latest=current)
    before = (github.latest, list(github.commands))
    with pytest.raises((ValueError, PermissionError)):
        module.prepare_rollback(
            github, repository="xindong/aris", current_tag="v2.0.0",
            previous_stable=predecessor, expected_previous_id=1, approved=True,
        )
    assert github.latest is before[0]
    assert not any(command[0] == "stable-latest" for command in github.commands)
    module.prepare_rollback(
        github, repository="xindong/aris", current_tag="v2.0.0",
        previous_stable="v1.9.0", expected_previous_id=1, approved=True,
    )
    assert github.latest is current


def test_local_dry_run_emits_public_fixture_without_private_key(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    module = _load_release_update()
    payload, bootstrap, setup = _inputs(tmp_path)
    output = tmp_path / "fixture"
    summary = module.local_dry_run(
        version="1.2.3", repository="xindong/aris", build_id="7167e34",
        published_at="2026-07-29T00:00:00Z", payload_dir=payload,
        bootstrap_path=bootstrap, installer_path=setup, output_dir=output,
    )
    captured = capsys.readouterr()
    assert "PRIVATE" not in captured.out + captured.err
    assert not any("private" in path.name.lower() or "secret" in path.name.lower() for path in output.rglob("*"))
    public = base64.b64decode(summary["public_key_b64"], validate=True)
    module.verify_manifest_bundle(
        output / "manifest-win-x64-testing.json",
        output / "manifest-win-x64-testing.json.sig",
        output / "Aris-1.2.3-win-x64-update.zip", public,
        channel="testing", version="1.2.3", repository="xindong/aris",
    )
    assert os.environ.get("ARIS_UPDATE_SIGNING_KEY_B64") is None

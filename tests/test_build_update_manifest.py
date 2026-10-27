from __future__ import annotations

import base64
import hashlib
import json
from pathlib import Path

import pytest
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.hazmat.primitives.serialization import Encoding, NoEncryption, PrivateFormat

from tools.build_update_manifest import build_manifest


def _install_test_key(monkeypatch: pytest.MonkeyPatch) -> Ed25519PrivateKey:
    key = Ed25519PrivateKey.generate()
    raw = key.private_bytes(Encoding.Raw, PrivateFormat.Raw, NoEncryption())
    monkeypatch.setenv("ARIS_UPDATE_SIGNING_KEY_B64", base64.b64encode(raw).decode("ascii"))
    return key


def test_new_repository_signs_separate_manifests_for_one_identical_update(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    key = _install_test_key(monkeypatch)
    asset = tmp_path / "Aris-1.2.0-win-x64-update.zip"
    asset.write_bytes(b"one shared immutable package")
    current, signature = build_manifest(
        channel="stable", version="1.2.0", asset_path=asset,
        repository="xindong/harken-search", build_id="new-public-commit",
        published_at="2026-10-28T00:00:00Z", allow_stable=True,
    )
    assert current.name == "manifest-harken-search-win-x64-stable.json"
    legacy = tmp_path / "manifest-win-x64-stable.json"
    documents = []
    for manifest in (current, legacy):
        data = manifest.read_bytes()
        detached = manifest.with_name(manifest.name + ".sig").read_text(encoding="ascii")
        key.public_key().verify(bytes.fromhex(detached), data)
        documents.append(json.loads(data))
    assert signature.name == current.name + ".sig"
    assert documents[0]["asset"]["url"].startswith("https://github.com/xindong/harken-search/")
    assert documents[1]["asset"]["url"].startswith("https://github.com/xindong/aris/")
    for document in documents:
        assert document["asset"]["sha256"] == hashlib.sha256(asset.read_bytes()).hexdigest()
        assert document["asset"]["size"] == asset.stat().st_size
        del document["asset"]["url"]
    assert documents[0] == documents[1]


def test_testing_manifest_is_compact_signed_and_hash_bound(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    key = _install_test_key(monkeypatch)
    asset = tmp_path / "Aris-1.2.0-win-x64-update.zip"
    asset.write_bytes(b"x" * 42)

    manifest_path, signature_path = build_manifest(
        channel="testing",
        version="1.2.0",
        asset_path=asset,
        repository="xindong/aris",
        build_id="abc123",
        published_at="2026-07-28T00:00:00Z",
        allow_stable=False,
    )

    payload = manifest_path.read_bytes()
    key.public_key().verify(bytes.fromhex(signature_path.read_text(encoding="ascii")), payload)
    document = json.loads(payload)
    assert b"\n" not in payload
    assert document["asset"]["size"] == 42
    assert document["asset"]["sha256"] == hashlib.sha256(b"x" * 42).hexdigest()
    assert document["asset"]["url"] == (
        "https://github.com/xindong/aris/releases/download/v1.2.0/"
        "Aris-1.2.0-win-x64-update.zip"
    )
    from tools.release_update import verify_manifest_bytes
    verified = verify_manifest_bytes(
        payload, signature_path.read_bytes(), asset.read_bytes(),
        key.public_key().public_bytes_raw(), channel="testing",
        version="1.2.0", repository="xindong/aris",
    )
    assert verified == document
    assert manifest_path.name == "manifest-win-x64-testing.json"
    assert signature_path.name == "manifest-win-x64-testing.json.sig"


def test_stable_manifest_requires_explicit_release_approval(tmp_path: Path) -> None:
    with pytest.raises(ValueError, match="allow-stable"):
        build_manifest(
            channel="stable",
            version="1.2.0",
            asset_path=tmp_path / "missing.zip",
            repository="xindong/aris",
            build_id="abc123",
            published_at="2026-07-28T00:00:00Z",
            allow_stable=False,
        )


def test_signing_key_is_required_and_must_be_32_bytes(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    asset = tmp_path / "Aris-1.2.0-win-x64-update.zip"
    asset.write_bytes(b"payload")
    monkeypatch.delenv("ARIS_UPDATE_SIGNING_KEY_B64", raising=False)
    with pytest.raises(ValueError, match="ARIS_UPDATE_SIGNING_KEY_B64"):
        build_manifest(
            channel="testing",
            version="1.2.0",
            asset_path=asset,
            repository="xindong/aris",
            build_id="abc123",
            published_at="2026-07-28T00:00:00Z",
            allow_stable=False,
        )

    monkeypatch.setenv(
        "ARIS_UPDATE_SIGNING_KEY_B64", base64.b64encode(b"too-short").decode("ascii")
    )
    with pytest.raises(ValueError, match="32 raw bytes"):
        build_manifest(
            channel="testing",
            version="1.2.0",
            asset_path=asset,
            repository="xindong/aris",
            build_id="abc123",
            published_at="2026-07-28T00:00:00Z",
            allow_stable=False,
        )

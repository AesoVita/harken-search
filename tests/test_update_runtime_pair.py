from __future__ import annotations

import hashlib
import json
import re
import struct
import subprocess
import sys
from pathlib import Path

from tools.generate_runtime_pair import signing_stable_pe_identity


ROOT = Path(__file__).resolve().parents[1]
GENERATOR = ROOT / "tools" / "generate_runtime_pair.py"
CMAKE = ROOT / "CMakeLists.txt"
AUTHORITY = ROOT / "src" / "platform" / "update_bootstrap_authority_win32.cpp"
KEY_ID = "aris-stable-2026-07-30"
PUBLIC_KEY = "11" * 32


def minimal_pe(certificate: bytes = b"") -> bytes:
    image = bytearray(512)
    image[:2] = b"MZ"
    struct.pack_into("<I", image, 0x3C, 0x80)
    image[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<H", image, 0x80 + 20, 0xE0)
    optional = 0x80 + 24
    struct.pack_into("<H", image, optional, 0x10B)
    if certificate:
        assert len(certificate) >= 8 and len(certificate) % 8 == 0
        struct.pack_into("<I", image, optional + 64, 0x12345678)
        struct.pack_into("<II", image, optional + 128, len(image), len(certificate))
        image.extend(certificate)
    return bytes(image)


def run_generator(*args: object) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, "-B", str(GENERATOR), *(str(value) for value in args)],
        cwd=ROOT,
        text=True,
        capture_output=True,
        check=False,
    )


def metadata_args(config: str = "Release") -> list[str]:
    return [
        "--source-commit", "a" * 40,
        "--build-id", "build-1da",
        "--channel", "qa",
        "--config", config,
        "--key-id", KEY_ID,
        "--public-key-hex", PUBLIC_KEY,
    ]


def verify_args(config: str = "Release", **overrides: str) -> list[str]:
    values = {
        "source_commit": "a" * 40,
        "build_id": "build-1da",
        "channel": "qa",
        "config": config,
        "key_id": KEY_ID,
        "public_key_hex": PUBLIC_KEY,
    }
    values.update(overrides)
    return [
        "--source-commit", values["source_commit"],
        "--build-id", values["build_id"],
        "--channel", values["channel"],
        "--config", values["config"],
        "--key-id", values["key_id"],
        "--public-key-hex", values["public_key_hex"],
    ]


def test_bootstrap_header_requires_complete_strict_provisioning(tmp_path: Path) -> None:
    bootstrap = tmp_path / "HarkenSearchBootstrap.exe"
    bootstrap.write_bytes(b"fresh-bootstrap")
    output = tmp_path / "generated" / "HarkenSearchRuntimePairIdentity.h"

    missing = run_generator(
        "bootstrap-header", "--bootstrap", bootstrap, "--output", output,
        *metadata_args()[:-4],
    )
    assert missing.returncode != 0
    assert not output.exists()

    single = run_generator(
        "bootstrap-header", "--bootstrap", bootstrap, "--output", output,
        *metadata_args()[:-2],
    )
    assert single.returncode != 0
    assert not output.exists()

    malformed = run_generator(
        "bootstrap-header", "--bootstrap", bootstrap, "--output", output,
        *metadata_args()[:-1], "00" * 32,
    )
    assert malformed.returncode != 0
    assert not output.exists()


def test_generator_emits_deterministic_closed_pair_and_rejects_tamper(
    tmp_path: Path,
) -> None:
    aris = tmp_path / "HarkenSearch.exe"
    bootstrap = tmp_path / "HarkenSearchBootstrap.exe"
    aris.write_bytes(b"aris-current-bytes")
    bootstrap.write_bytes(minimal_pe())
    header = tmp_path / "generated" / "Release" / "HarkenSearchRuntimePairIdentity.h"
    receipt = tmp_path / "harken-runtime-pair.json"
    bootstrap_sig = tmp_path / "HarkenSearchBootstrap.exe.sig"

    generated = run_generator(
        "bootstrap-header", "--bootstrap", bootstrap, "--output", header,
        *metadata_args(),
    )
    assert generated.returncode == 0, generated.stderr
    header_text = header.read_text(encoding="utf-8")
    image_digest, image_size = signing_stable_pe_identity(bootstrap)
    assert image_digest in header_text
    assert f"kBootstrapImageSize = {image_size}ULL" in header_text
    assert PUBLIC_KEY not in header_text

    finalized = run_generator(
        "finalize", "--aris", aris, "--bootstrap", bootstrap,
        "--bootstrap-sig", bootstrap_sig, "--receipt", receipt,
        *metadata_args(),
    )
    assert finalized.returncode == 0, finalized.stderr
    first = receipt.read_bytes()
    document = json.loads(first.decode("utf-8"))
    assert list(document) == sorted(document)
    assert document["schemaVersion"] == 1
    assert document["arisSha256"] == hashlib.sha256(aris.read_bytes()).hexdigest()
    assert document["bootstrapSha256"] == hashlib.sha256(bootstrap.read_bytes()).hexdigest()
    assert bootstrap_sig.read_text(encoding="ascii") == document["bootstrapSha256"] + "\n"
    assert PUBLIC_KEY not in first.decode("utf-8")

    repeated = run_generator(
        "finalize", "--aris", aris, "--bootstrap", bootstrap,
        "--bootstrap-sig", bootstrap_sig, "--receipt", receipt,
        *metadata_args(),
    )
    assert repeated.returncode == 0, repeated.stderr
    assert receipt.read_bytes() == first

    verified = run_generator(
        "verify", "--aris", aris, "--bootstrap", bootstrap,
        "--bootstrap-sig", bootstrap_sig, "--receipt", receipt,
        *verify_args(),
    )
    assert verified.returncode == 0, verified.stderr

    bootstrap.write_bytes(b"stale-bootstrap")
    rejected = run_generator(
        "verify", "--aris", aris, "--bootstrap", bootstrap,
        "--bootstrap-sig", bootstrap_sig, "--receipt", receipt,
        *verify_args(),
    )
    assert rejected.returncode != 0

    document["unexpected"] = True
    receipt.write_text(json.dumps(document), encoding="utf-8")
    unknown = run_generator(
        "verify", "--aris", aris, "--bootstrap", bootstrap,
        "--bootstrap-sig", bootstrap_sig, "--receipt", receipt,
        *verify_args(),
    )
    assert unknown.returncode != 0


def test_bootstrap_identity_survives_authenticode_overlay(tmp_path: Path) -> None:
    unsigned = tmp_path / "unsigned" / "HarkenSearchBootstrap.exe"
    signed = tmp_path / "signed" / "HarkenSearchBootstrap.exe"
    unsigned.parent.mkdir()
    signed.parent.mkdir()
    unsigned.write_bytes(minimal_pe())
    signed.write_bytes(minimal_pe(b"certificate-data"))
    unsigned_header = tmp_path / "unsigned.h"
    signed_header = tmp_path / "signed.h"

    for bootstrap, output in (
        (unsigned, unsigned_header),
        (signed, signed_header),
    ):
        result = run_generator(
            "bootstrap-header", "--bootstrap", bootstrap, "--output", output,
            *metadata_args(),
        )
        assert result.returncode == 0, result.stderr

    def embedded_identity(path: Path) -> tuple[str, str]:
        text = path.read_text(encoding="utf-8")
        digest = re.search(r'kBootstrapImageSha256 = "([0-9a-f]{64})"', text)
        size = re.search(r"kBootstrapImageSize = ([0-9]+)ULL", text)
        assert digest and size
        return digest.group(1), size.group(1)

    assert hashlib.sha256(unsigned.read_bytes()).digest() != hashlib.sha256(
        signed.read_bytes()
    ).digest()
    assert embedded_identity(unsigned_header) == embedded_identity(signed_header)


def test_bootstrap_identity_rejects_nonterminal_certificate_table(
    tmp_path: Path,
) -> None:
    bootstrap = tmp_path / "HarkenSearchBootstrap.exe"
    bootstrap.write_bytes(minimal_pe(b"certificate-data") + b"unbound-overlay")

    result = run_generator(
        "bootstrap-header", "--bootstrap", bootstrap,
        "--output", tmp_path / "identity.h", *metadata_args(),
    )

    assert result.returncode != 0
    assert "terminal overlay" in result.stderr


def test_verify_binds_expected_provenance_metadata(tmp_path: Path) -> None:
    aris = tmp_path / "HarkenSearch.exe"
    bootstrap = tmp_path / "HarkenSearchBootstrap.exe"
    receipt = tmp_path / "harken-runtime-pair.json"
    bootstrap_sig = tmp_path / "HarkenSearchBootstrap.exe.sig"
    aris.write_bytes(b"aris")
    bootstrap.write_bytes(b"bootstrap")
    assert run_generator(
        "finalize", "--aris", aris, "--bootstrap", bootstrap,
        "--bootstrap-sig", bootstrap_sig, "--receipt", receipt,
        *metadata_args(),
    ).returncode == 0
    for field, value in {
        "source_commit": "0" * 40,
        "build_id": "wrong-build",
        "channel": "stable",
        "config": "Debug",
        "key_id": "wrong-key",
        "public_key_hex": "22" * 32,
    }.items():
        result = run_generator(
            "verify", "--aris", aris, "--bootstrap", bootstrap,
            "--bootstrap-sig", bootstrap_sig, "--receipt", receipt,
            *verify_args(**{field: value}),
        )
        assert result.returncode != 0, field


def test_verify_rejects_pe_with_stale_embedded_bootstrap_identity(
    tmp_path: Path,
) -> None:
    aris = tmp_path / "HarkenSearch.exe"
    bootstrap = tmp_path / "HarkenSearchBootstrap.exe"
    receipt = tmp_path / "harken-runtime-pair.json"
    bootstrap_sig = tmp_path / "HarkenSearchBootstrap.exe.sig"
    aris.write_bytes(b"MZ" + b"0" * 64 + b"aris-pe")
    bootstrap.write_bytes(minimal_pe())
    assert run_generator(
        "finalize", "--aris", aris, "--bootstrap", bootstrap,
        "--bootstrap-sig", bootstrap_sig, "--receipt", receipt,
        *metadata_args(),
    ).returncode == 0

    result = run_generator(
        "verify", "--aris", aris, "--bootstrap", bootstrap,
        "--bootstrap-sig", bootstrap_sig, "--receipt", receipt,
        *verify_args(),
    )

    assert result.returncode != 0
    assert "embedded Bootstrap identity" in result.stderr


def test_verify_rejects_noncanonical_receipt_bytes(tmp_path: Path) -> None:
    aris = tmp_path / "HarkenSearch.exe"
    bootstrap = tmp_path / "HarkenSearchBootstrap.exe"
    receipt = tmp_path / "harken-runtime-pair.json"
    bootstrap_sig = tmp_path / "HarkenSearchBootstrap.exe.sig"
    aris.write_bytes(b"aris")
    bootstrap.write_bytes(b"bootstrap")
    assert run_generator(
        "finalize", "--aris", aris, "--bootstrap", bootstrap,
        "--bootstrap-sig", bootstrap_sig, "--receipt", receipt,
        *metadata_args(),
    ).returncode == 0
    document = json.loads(receipt.read_text(encoding="utf-8"))
    variants = [
        (json.dumps(document, ensure_ascii=False, indent=2) + "\n").encode("utf-8"),
        (json.dumps(dict(reversed(list(document.items()))), ensure_ascii=False,
                    separators=(",", ":")).encode("utf-8")),
        (receipt.read_bytes() + b" \n"),
        (b"\xef\xbb\xbf" + receipt.read_bytes()),
    ]
    for raw in variants:
        receipt.write_bytes(raw)
        result = run_generator(
            "verify", "--aris", aris, "--bootstrap", bootstrap,
            "--bootstrap-sig", bootstrap_sig, "--receipt", receipt,
            *verify_args(),
        )
        assert result.returncode != 0


def test_cmake_builds_config_specific_pair_from_current_bootstrap() -> None:
    source = CMAKE.read_text(encoding="utf-8")
    assert "generate_runtime_pair.py" in source
    assert "bootstrap-header" in source
    assert "finalize" in source
    assert "$<TARGET_FILE:HarkenSearchBootstrap>" in source
    assert "$<CONFIG>" in source
    assert "HarkenSearchRuntimePairIdentity.h" in source
    assert "harken-runtime-pair.json" in source
    assert "ARIS_BOOTSTRAP_PACKAGE_KEY_ID" in source
    assert "ARIS_BOOTSTRAP_PACKAGE_PUBLIC_KEY_HEX" in source
    assert "add_dependencies(HarkenSearch HarkenSearchRuntimePairIdentity)" in source


def test_authority_verifies_sibling_and_updater_owned_copy_before_effects() -> None:
    source = AUTHORITY.read_text(encoding="utf-8")
    authorize = source.index("UpdateBootstrapAuthorizationResult Authorize")
    sibling = source.index("ObserveRuntimePairFile(", authorize)
    copy = source.index("CopyVerifiedBootstrap(", sibling)
    extraction = source.index("ExtractPayload(pinnedArchive", copy)
    capability = source.index("IssueCapability(state)", extraction)
    submit = source.index("UpdateBootstrapSubmitResult Submit")
    sibling_recheck = source.index("ObserveRuntimePairFile(", submit)
    copy_recheck = source.index("ObserveRuntimePairFile(", sibling_recheck + 1)
    consume = source.index("consumed->exchange", copy_recheck)
    launch = source.index("LaunchBootstrap(*state)", consume)

    assert authorize < sibling < copy < extraction < capability
    assert submit < sibling_recheck < copy_recheck < consume < launch
    assert "HarkenSearchRuntimePairIdentity.h" in source
    assert "FILE_FLAG_OPEN_REPARSE_POINT" in source
    assert "BCRYPT_SHA256_ALGORITHM" in source
    assert "HashSigningStableBootstrapImage" in source
    assert "kBootstrapImageSha256" in source
    assert "kBootstrapImageSize" in source
    assert "CREATE_SUSPENDED" in source
    assert "SameRuntimePairFile" in source
    assert "state->bootstrapPath = launchCopy" in source
    assert "ResumeThread(info.hThread)" in source

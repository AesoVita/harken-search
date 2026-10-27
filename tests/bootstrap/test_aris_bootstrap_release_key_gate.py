from pathlib import Path
import re
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]


class ArisBootstrapReleaseKeyGateContract(unittest.TestCase):
    def test_production_entrypoint_consumes_only_opaque_handoff_authority(self) -> None:
        admission_header = (
            ROOT / "src/bootstrap/HarkenSearchBootstrapAdmission.h"
        ).read_text(encoding="utf-8-sig")
        main = (ROOT / "src/bootstrap/HarkenSearchBootstrapMain.cpp").read_text(
            encoding="utf-8-sig"
        )
        self.assertNotRegex(
            admission_header,
            re.compile(
                r"\bAdmitBootstrapInvocation\s*\(\s*"
                r"const\s+PackagePlanAdmission\s*&\s*,\s*"
                r"const\s+InvocationEnvelope\s*&"
            ),
            "production headers must not expose raw caller-constructed invocation admission",
        )
        self.assertIn("VerifyPackagePlanBinding", main)
        self.assertIn("SubmitVerifiedPackagePlan", main)
        self.assertNotIn("AdmitBootstrapInvocation(package, invocation)", main)
        self.assertNotRegex(
            admission_header,
            re.compile(
                r"\bAdmitAuthenticatedBootstrapRecovery\s*\([^;]*"
                r"const\s+BootstrapChannelResult\s*&",
                re.DOTALL,
            ),
            "restart admission must consume opaque submitted authority, not a forgeable channel DTO",
        )
        self.assertNotIn(
            "durable, trust, channel, base, staging, transaction",
            main,
        )

    def test_bootstrap_target_does_not_inherit_media_foundation_link_set(self) -> None:
        source = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8-sig")
        self.assertNotRegex(
            source,
            re.compile(
                r"link_libraries\(\s*mfplat\s+mfreadwrite\s+mfuuid\s*\)",
                re.MULTILINE,
            ),
            "Bootstrap must not inherit the application's Media Foundation/audio link set",
        )

    def test_product_target_links_the_shared_bootstrap_core(self) -> None:
        source = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8-sig")
        target_match = re.search(
            r"add_executable\(HarkenSearchBootstrap WIN32(?P<sources>.*?)\n\s*\)",
            source,
            re.DOTALL,
        )
        self.assertIsNotNone(target_match, "HarkenSearchBootstrap target must be present")
        assert target_match is not None
        self.assertNotIn(
            "src/bootstrap/HarkenSearchBootstrapPlan.cpp",
            target_match.group("sources"),
            "the product target must not compile a second copy of the pure core",
        )
        self.assertRegex(
            source,
            re.compile(
                r"target_link_libraries\(HarkenSearchBootstrap PRIVATE\s+"
                r"HarkenSearchBootstrapCore\b",
                re.DOTALL,
            ),
            "the executable must consume the same pure-core target as integration tests",
        )

    def test_release_composition_requires_provisioned_package_key(self) -> None:
        source = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8-sig")
        self.assertIn(
            "ARIS_BOOTSTRAP_RELEASE_BUILD",
            source,
            "release composition must have an explicit provisioning mode",
        )
        self.assertIn(
            "HarkenSearchBootstrap release builds require",
            source,
            "the release gate must fail closed when key material is absent",
        )
        self.assertIn("ARIS_BOOTSTRAP_PACKAGE_KEY_ID", source)
        self.assertIn("ARIS_BOOTSTRAP_PACKAGE_PUBLIC_KEY_HEX", source)
        self.assertRegex(
            source,
            re.compile(
                r"if\(\(ARIS_BOOTSTRAP_RELEASE_BUILD\s+OR\s+"
                r"ARIS_PUBLIC_RELEASE\)\s+AND"
            ),
            "the repository's public-release mode must not bypass Bootstrap key provisioning",
        )

    def test_multiconfig_release_target_has_a_compile_time_key_gate(self) -> None:
        source = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8-sig")
        main = (ROOT / "src/bootstrap/HarkenSearchBootstrapMain.cpp").read_text(
            encoding="utf-8-sig"
        )
        self.assertIn(
            "$<$<CONFIG:Release>:ARIS_BOOTSTRAP_RELEASE_CONFIG=1>",
            source,
        )
        self.assertIn("ARIS_BOOTSTRAP_RELEASE_CONFIG", main)
        self.assertIn("injected package verification key", main)

    def test_release_configure_rejects_missing_partial_and_malformed_keys(self) -> None:
        valid_public_key = (
            "d75a980182b10ab7d54bfed3c964073a"
            "0ee172f3daa62325af021a68f707511a"
        )
        strict_rejections = (
            "00" * 32,
            "00" * 31 + "80",
            "01" + "00" * 31,
            "01" + "00" * 30 + "80",
            "e0eb7a7c3b41b8ae1656e3faf19fc46ada098deb9c32b1fd866205165f49b800",
            "e0eb7a7c3b41b8ae1656e3faf19fc46ada098deb9c32b1fd866205165f49b880",
            "5f9c95bca3508c24b1d0b1559c83ef5b04445cc4581c8e86d8224eaa78aad612",
            "5f9c95bca3508c24b1d0b1559c83ef5b04445cc4581c8e86d8224eaa78aad692",
            "ec" + "ff" * 30 + "7f",
            "ec" + "ff" * 31,
            "ed" + "ff" * 30 + "7f",
            "ed" + "ff" * 31,
            "ee" + "ff" * 30 + "7f",
            "ee" + "ff" * 31,
        )
        cases = (
            ("missing", "", "", "HarkenSearchBootstrap release builds require"),
            ("key-id-only", "fixture-2026", "", "must be supplied together"),
            ("public-key-only", "", valid_public_key, "must be supplied together"),
            ("short-key", "fixture-2026", "00", "must be a nonzero 32-byte"),
            (
                "uppercase-key",
                "fixture-2026",
                valid_public_key.upper(),
                "must be a nonzero 32-byte",
            ),
            ("zero-key", "fixture-2026", "0" * 64, "must be a nonzero 32-byte"),
            *tuple(
                (
                    f"strict-rejection-{index}",
                    "fixture-2026",
                    public_key,
                    "must be a strict Ed25519 public key",
                )
                for index, public_key in enumerate(strict_rejections)
                if public_key != "00" * 32
            ),
            ("valid", "fixture-2026", valid_public_key, None),
        )
        with tempfile.TemporaryDirectory(prefix="aris-bootstrap-key-gate-") as temporary:
            temporary_root = Path(temporary)
            for name, key_id, public_key, rejection in cases:
                with self.subTest(name=name):
                    build = temporary_root / name
                    command = [
                        "cmake",
                        "-S",
                        str(ROOT),
                        "-B",
                        str(build),
                        "-DARIS_BOOTSTRAP_RELEASE_BUILD=ON",
                        f"-DARIS_BOOTSTRAP_PACKAGE_KEY_ID={key_id}",
                        f"-DARIS_BOOTSTRAP_PACKAGE_PUBLIC_KEY_HEX={public_key}",
                    ]
                    completed = subprocess.run(
                        command,
                        cwd=ROOT,
                        capture_output=True,
                        text=True,
                        timeout=120,
                        check=False,
                    )
                    diagnostic = completed.stdout + completed.stderr
                    if rejection is None:
                        self.assertEqual(0, completed.returncode, diagnostic)
                    else:
                        self.assertNotEqual(0, completed.returncode, diagnostic)
                        self.assertIn(rejection, " ".join(diagnostic.split()))


if __name__ == "__main__":
    unittest.main()

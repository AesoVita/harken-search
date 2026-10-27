"""REAPER DLL display identity and upgrade trust boundary."""

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
RC = (ROOT / "reaper_extension/reaper_harken.rc").read_text(encoding="utf-8")
DOCTOR = (ROOT / "src/ReaperBridgeDoctor.cpp").read_text(encoding="utf-8")


def version_value(name: str) -> str:
    match = re.search(r'VALUE "' + re.escape(name) + r'", "([^"]*)"', RC)
    if not match:
        raise AssertionError(f"missing DLL version value: {name}")
    return match.group(1).removesuffix(chr(92) + "0")


class ReaperBridgeIdentityContract(unittest.TestCase):
    def test_new_dll_displays_harken_search_without_renaming_its_address(self):
        self.assertEqual(version_value("CompanyName"), "HarkenSearch")
        self.assertEqual(version_value("ProductName"), "HarkenSearch")
        self.assertEqual(version_value("FileDescription"), "HarkenSearch REAPER Extension")
        self.assertEqual(version_value("OriginalFilename"), "reaper_harken.dll")
        self.assertEqual(version_value("InternalName"), "reaper_auro")

    def test_doctor_trusts_new_bundle_and_recognizes_old_installed_identity(self):
        probe = DOCTOR.split("BridgeBinaryInfo ProbeBridgeBinary(", 1)[1].split(
            "InstallStatus ProbeReaperBridge(", 1
        )[0]
        current = probe.split("info.trustedSource =", 1)[1].split(";", 1)[0]
        legacy = probe.split("info.trustedSource =", 1)[1].split(";", 1)[1]
        self.assertIn('info.productName == "HarkenSearch"', current)
        self.assertIn('info.fileDescription == "HarkenSearch REAPER Extension"', current)
        self.assertNotIn('info.productName == "Aris"', current)
        self.assertIn('info.productName == "Aris"', legacy)
        self.assertIn('info.fileDescription == "Aris REAPER Extension"', legacy)
        self.assertIn("info.legacySignature", legacy)
        self.assertIn("info.legacySignature = info.trustedSource ||", legacy)


if __name__ == "__main__":
    unittest.main()

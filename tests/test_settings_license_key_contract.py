"""Retired license secrets must never enter the settings serialization path."""

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class SettingsLicenseKeyContractTests(unittest.TestCase):
    def test_settings_save_does_not_serialize_retired_license_key(self):
        source = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
        start = source.index("bool TrySaveAppSettings(")
        end = source.index("void MarkSettingsDirty(", start)
        save = source[start:end]
        self.assertNotIn("state.licenseState.licenseKey", save)
        self.assertNotIn('"licenseKey"', save)


if __name__ == "__main__":
    unittest.main()

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src" / "DawBridgeInstaller.cpp").read_text(encoding="utf-8")


class DawBridgeInstallerPlatformContract(unittest.TestCase):
    def test_public_registry_is_reaper_only(self):
        self.assertEqual(1, SOURCE.count("registry.push_back("))
        self.assertIn("registry.push_back(MakeReaperExtensionInstaller());", SOURCE)
        self.assertIn('inst.targetId                   = "reaper-extension";', SOURCE)
        self.assertIn("inst.requiresAdmin              = false;", SOURCE)
        self.assertIn("ProbeReaperBridge()", SOURCE)
        self.assertIn("InstallReaperBridge({}, dryRun)", SOURCE)

    def test_unsupported_installer_family_is_absent(self):
        for token in (
            "MakeNuendoSkiInstaller",
            "nuendo-ski",
            "Install_NuendoSki",
            "AuroNuendoSkiBridge",
        ):
            self.assertNotIn(token, SOURCE)


if __name__ == "__main__":
    unittest.main()

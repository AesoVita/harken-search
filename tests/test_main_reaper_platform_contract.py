import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class MainReaperPlatformContractTests(unittest.TestCase):
    def test_steinberg_install_discovery_uses_directory_iterator_pal(self):
        source = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
        match = re.search(
            r"static std::wstring FindInstalledSteinbergExecutable\(const DawTarget& target\) \{.*?\n}\n",
            source,
            re.DOTALL,
        )
        self.assertIsNotNone(match)
        function = match.group(0)
        self.assertIn("Platform::FileSystem::DirectoryIterator", function)
        self.assertIn("Platform::FileSystem::DirectoryReadStatus::Entry", function)
        for native_call in ("FindFirstFileW", "FindNextFileW", "FindClose"):
            self.assertNotIn(native_call, function)

    def test_reaper_install_source_path_uses_system_pal_exe_dir(self):
        source = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
        match = re.search(
            r"static bool EnsureReaperExtensionInstalled\(.*?\n}\n",
            source,
            re.DOTALL,
        )
        self.assertIsNotNone(match)
        function = match.group(0)
        self.assertIn("Platform::GetExeDir()", function)
        self.assertNotIn("GetModuleFileNameW", function)


if __name__ == "__main__":
    unittest.main()

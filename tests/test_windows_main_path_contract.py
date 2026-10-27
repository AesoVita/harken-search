import pathlib
import unittest


class WindowsMainPathContractTests(unittest.TestCase):
    def test_runtime_directory_creation_is_owned_by_platform_pal(self):
        source = (pathlib.Path(__file__).resolve().parents[1] / "src" / "WindowsMain.cpp").read_text(
            encoding="utf-8"
        )

        self.assertNotIn("std::filesystem::create_directories", source)
        self.assertEqual(source.count("Platform::FileSystem::CreateDirectories("), 3)

        platform_header = (
            pathlib.Path(__file__).resolve().parents[1] / "src" / "platform" / "filesystem.h"
        ).read_text(encoding="utf-8")
        self.assertIn("bool CreateDirectories(const std::string& pathUtf8);", platform_header)

    def test_startup_resource_reads_are_owned_by_platform_pal(self):
        root = pathlib.Path(__file__).resolve().parents[1]
        source = (root / "src" / "WindowsMain.cpp").read_text(encoding="utf-8")
        header = (root / "src" / "platform" / "filesystem.h").read_text(encoding="utf-8")

        self.assertNotIn("std::ifstream", source)
        self.assertEqual(source.count("Platform::FileSystem::ReadFileBytes("), 2)
        self.assertIn(
            "std::vector<uint8_t> ReadFileBytes(const std::string& pathUtf8);",
            header,
        )


if __name__ == "__main__":
    unittest.main()

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DOCTOR = (ROOT / "src" / "ReaperBridgeDoctor.cpp").read_text(encoding="utf-8")
FILESYSTEM_HEADER = (ROOT / "src" / "platform" / "filesystem.h").read_text(
    encoding="utf-8"
)
FILESYSTEM_IMPL = (ROOT / "src" / "platform" / "filesystem_std.cpp").read_text(
    encoding="utf-8"
)


class ReaperBridgeDoctorFilesystemContract(unittest.TestCase):
    def test_directory_queries_and_creation_use_filesystem_pal(self):
        self.assertIn('#include "platform/filesystem.h"', DOCTOR)
        self.assertIn("Platform::FileSystem::IsDirectory(path.u8string())", DOCTOR)
        self.assertIn("Platform::FileSystem::EnsureDirectoryExists", DOCTOR)
        self.assertIn("Platform::FileSystem::ProbeRegularFile(path.u8string())", DOCTOR)
        self.assertIn("info.exists = file.isRegularFile", DOCTOR)
        self.assertIn("info.sizeBytes = file.sizeBytes", DOCTOR)
        self.assertNotIn("fs::is_directory", DOCTOR)
        self.assertNotIn("fs::create_directories", DOCTOR)
        self.assertNotIn("fs::is_regular_file", DOCTOR)
        self.assertNotIn("fs::file_size", DOCTOR)

    def test_ensure_directory_api_preserves_create_directories_contract(self):
        self.assertIn(
            "bool EnsureDirectoryExists(const std::string& pathUtf8);",
            FILESYSTEM_HEADER,
        )
        self.assertIn("bool EnsureDirectoryExists(", FILESYSTEM_IMPL)
        self.assertIn("std::filesystem::create_directories(path, ec);", FILESYSTEM_IMPL)
        self.assertIn("std::filesystem::is_directory(path, ec)", FILESYSTEM_IMPL)
        self.assertIn("bool CreateDirectories(", FILESYSTEM_IMPL)
        self.assertIn("struct RegularFileInfo", FILESYSTEM_HEADER)
        self.assertIn("RegularFileInfo ProbeRegularFile(", FILESYSTEM_HEADER)
        self.assertIn("std::numeric_limits<uint64_t>::max()", FILESYSTEM_IMPL)
        self.assertIn("info.sizeAvailable = true", FILESYSTEM_IMPL)

    def test_other_doctor_native_work_stays_outside_this_slice(self):
        for token in (
        ):
            self.assertIn(token, DOCTOR)
        self.assertIn('result.failureCode = "target-directory-create-failed";', DOCTOR)


if __name__ == "__main__":
    unittest.main()

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = (ROOT / "src" / "platform" / "filesystem.h").read_text(encoding="utf-8")
FILESYSTEM = (ROOT / "src" / "platform" / "filesystem_std.cpp").read_text(encoding="utf-8")
RUNTIME = (ROOT / "src" / "HostSendRuntime.cpp").read_text(encoding="utf-8")
CMAKE = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
BEHAVIOR_CMAKE = (ROOT / "cmake" / "harken_search_behavior_tests.cmake").read_text(encoding="utf-8")


class HostSendRuntimePlatformContract(unittest.TestCase):
    def test_equivalent_pal_is_native_free_and_error_safe(self):
        self.assertIn("bool Equivalent(const std::string& leftPathUtf8,", HEADER)
        self.assertIn("const std::string& rightPathUtf8);", HEADER)
        self.assertNotIn("std::filesystem", HEADER)
        self.assertIn("std::filesystem::equivalent", FILESYSTEM)
        self.assertIn("return equivalent && !ec", FILESYSTEM)

    def test_runtime_owns_only_pure_path_operations(self):
        self.assertIn('#include "platform/filesystem.h"', RUNTIME)
        for token in (
            "Platform::FileSystem::ProbePath",
            "Platform::FileSystem::PathStatus::Error",
            "Platform::FileSystem::PathStatus::Missing",
            "Platform::FileSystem::EnsureDirectoryExists",
            "Platform::FileSystem::Equivalent",
            "Platform::FileSystem::CopyFile",
        ):
            self.assertIn(token, RUNTIME)
        self.assertIn("true)", RUNTIME)
        self.assertNotIn("outError.clear", RUNTIME)
        for token in ("std::filesystem::exists", "std::filesystem::create_directories",
                      "std::filesystem::equivalent", "std::filesystem::copy_file"):
            self.assertNotIn(token, RUNTIME)

    def test_runtime_tests_are_registered(self):
        self.assertIn("TestHostSendRuntimePlatformContract", CMAKE)
        self.assertIn("TestHostSendRuntime", BEHAVIOR_CMAKE)
        self.assertIn("tests/test_host_send_runtime.cpp", BEHAVIOR_CMAKE)
        self.assertIn("src/HostSendRuntime.cpp", BEHAVIOR_CMAKE)
        self.assertIn("src/platform/filesystem_std.cpp", BEHAVIOR_CMAKE)


if __name__ == "__main__":
    unittest.main()

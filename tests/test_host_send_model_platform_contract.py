import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = (ROOT / "src" / "platform" / "filesystem.h").read_text(encoding="utf-8")
FILESYSTEM = (ROOT / "src" / "platform" / "filesystem_std.cpp").read_text(encoding="utf-8")
HOST_SEND = (ROOT / "src" / "HostSendModel.cpp").read_text(encoding="utf-8")
CMAKE = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
BEHAVIOR_CMAKE = (ROOT / "cmake" / "harken_search_behavior_tests.cmake").read_text(encoding="utf-8")
STUB = (ROOT / "tests" / "test_host_send_platform_stub.cpp").read_text(encoding="utf-8")


class HostSendModelPlatformContract(unittest.TestCase):
    def test_typed_temporary_directory_api_preserves_error_code(self):
        for token in (
            "enum class TemporaryDirectoryStatus",
            "Success",
            "QueryFailed",
            "struct TemporaryDirectoryResult",
            "std::string pathUtf8",
            "std::error_code error",
            "TemporaryDirectoryResult QueryTemporaryDirectory();",
        ):
            self.assertIn(token, HEADER)
        self.assertIn("std::filesystem::temp_directory_path(ec)", FILESYSTEM)
        self.assertIn("result.error = ec", FILESYSTEM)
        self.assertIn("TemporaryDirectoryStatus::Success", FILESYSTEM)
        self.assertIn("QueryTemporaryDirectory()", FILESYSTEM)
        self.assertIn("return result.pathUtf8", FILESYSTEM)

    def test_host_send_uses_pal_and_preserves_priority_suffix_and_error(self):
        self.assertIn('#include "platform/filesystem.h"', HOST_SEND)
        self.assertIn('#include "platform/system.h"', HOST_SEND)
        self.assertIn('Platform::ReadEnvironmentVariable("LOCALAPPDATA")', HOST_SEND)
        self.assertIn("EnvironmentStringStatus::Success", HOST_SEND)
        self.assertIn("Platform::FileSystem::QueryTemporaryDirectory()", HOST_SEND)
        self.assertIn("std::filesystem::filesystem_error(", HOST_SEND)
        self.assertIn('"temp_directory_path", temporary.error', HOST_SEND)
        self.assertLess(HOST_SEND.index('root /= "Auro"'), HOST_SEND.index('root /= "HostSend"'))
        for token in ("_dupenv_s", "std::getenv", "getenv(",
                      "std::filesystem::temp_directory_path()"):
            self.assertNotIn(token, HOST_SEND)

    def test_ctest_and_configurable_stub_are_wired(self):
        self.assertIn("TestHostSendModelPlatformContract", CMAKE)
        self.assertIn("tests/test_host_send_platform_stub.cpp", BEHAVIOR_CMAKE)
        self.assertIn("void SetEnvironmentResult", STUB)
        self.assertIn("EnvironmentStringResult ReadEnvironmentVariable", STUB)


if __name__ == "__main__":
    unittest.main()

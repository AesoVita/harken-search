import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER_PATH = ROOT / "src" / "platform" / "executable_path.h"
WIN32_PATH = ROOT / "src" / "platform" / "executable_path_win32.cpp"
MACOS_PATH = ROOT / "src" / "platform" / "executable_path_macos.mm"
DOCTOR = (ROOT / "src" / "ReaperBridgeDoctor.cpp").read_text(encoding="utf-8")


class ReaperBridgeDoctorExecutablePathContract(unittest.TestCase):
    def test_public_result_api_is_native_free(self):
        self.assertTrue(HEADER_PATH.is_file())
        header = HEADER_PATH.read_text(encoding="utf-8")
        for status in (
            "Unsupported",
            "Success",
            "QueryFailed",
            "Truncated",
            "EncodingFailed",
        ):
            self.assertIn(status, header)
        self.assertIn("struct ExecutablePathResult", header)
        self.assertIn("std::string pathUtf8", header)
        self.assertIn("unsigned long systemError", header)
        self.assertIn("ExecutablePathResult GetExecutablePath();", header)
        for token in ("windows.h", "DWORD", "NSBundle", "GetModuleFileNameW"):
            self.assertNotIn(token, header)

    def test_win32_query_grows_to_os_limit_without_partial_success(self):
        self.assertTrue(WIN32_PATH.is_file())
        source = WIN32_PATH.read_text(encoding="utf-8")
        self.assertIn("constexpr std::size_t kInitialCapacity = 512", source)
        self.assertIn("constexpr std::size_t kMaximumCapacity = 32768", source)
        self.assertIn("SetLastError(ERROR_SUCCESS)", source)
        self.assertIn("GetModuleFileNameW(nullptr", source)
        self.assertIn("length == 0", source)
        self.assertIn("length < capacity", source)
        self.assertIn("length >= capacity", source)
        self.assertIn("ExecutablePathStatus::QueryFailed", source)
        self.assertIn("ExecutablePathStatus::Truncated", source)
        self.assertIn("ExecutablePathStatus::EncodingFailed", source)
        self.assertIn("ERROR_INSUFFICIENT_BUFFER", source)
        self.assertIn("WideToUtf8Strict", source)
        self.assertNotIn("MAX_PATH", source)

    def test_macos_maps_bundle_query_and_encoding_failures(self):
        self.assertTrue(MACOS_PATH.is_file())
        source = MACOS_PATH.read_text(encoding="utf-8")
        self.assertIn("[[NSBundle mainBundle] executablePath]", source)
        self.assertIn("UTF8String", source)
        self.assertIn("ExecutablePathStatus::QueryFailed", source)
        self.assertIn("ExecutablePathStatus::EncodingFailed", source)
        self.assertIn("ExecutablePathStatus::Success", source)
        self.assertNotIn("GetModuleFileNameW", source)

    def test_doctor_delegates_but_daw_and_other_native_work_stay_put(self):
        self.assertIn('#include "platform/executable_path.h"', DOCTOR)
        self.assertIn("Platform::GetExecutablePath()", DOCTOR)
        self.assertIn("ExecutablePathStatus::Success", DOCTOR)
        self.assertNotIn("GetModuleFileNameW", DOCTOR)
        for token in (
        ):
            self.assertIn(token, DOCTOR)


if __name__ == "__main__":
    unittest.main()

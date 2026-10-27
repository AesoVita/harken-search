import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CMAKE = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
HEADER = (ROOT / "src" / "platform" / "system.h").read_text(encoding="utf-8")
WIN32 = (ROOT / "src" / "platform" / "system_win32.cpp").read_text(encoding="utf-8")
MACOS = (ROOT / "src" / "platform" / "system_macos.mm").read_text(encoding="utf-8")
DOCTOR = (ROOT / "src" / "ReaperBridgeDoctor.cpp").read_text(encoding="utf-8")


class ReaperBridgeDoctorRegisteredExecutableContract(unittest.TestCase):
    def test_api_and_ctest_registration_are_native_free(self):
        for token in (
            "enum class RegisteredExecutableScope",
            "CurrentUser",
            "LocalMachine",
            "enum class RegisteredExecutablePathStatus",
            "ValueTooLarge",
            "struct RegisteredExecutablePathResult",
            "std::string executablePathUtf8",
            "RegisteredExecutablePathResult ReadRegisteredExecutablePath(",
        ):
            self.assertIn(token, HEADER)
        for token in ("windows.h", "HKEY", "RegGetValueW"):
            self.assertNotIn(token, HEADER)
        self.assertIn("TestReaperBridgeDoctorRegisteredExecutableContract", CMAKE)

    def test_windows_scoped_app_paths_query_and_status_mapping(self):
        for token in (
            "HKEY_CURRENT_USER",
            "HKEY_LOCAL_MACHINE",
            "Software\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\App Paths\\\\",
            "RRF_RT_REG_SZ",
            "wchar_t value[32768]",
            "ERROR_FILE_NOT_FOUND",
            "ERROR_PATH_NOT_FOUND",
            "ERROR_MORE_DATA",
            "RegisteredExecutablePathStatus::NotFound",
            "RegisteredExecutablePathStatus::ValueTooLarge",
            "TextEncoding::WideToUtf8Strict",
        ):
            self.assertIn(token, WIN32)

    def test_macos_is_unsupported(self):
        self.assertIn("RegisteredExecutablePathStatus::Unsupported", MACOS)
        self.assertNotIn("RegGetValueW", MACOS)

    def test_doctor_loops_scopes_and_keeps_file_exists_fallback(self):
        self.assertIn("Platform::ReadRegisteredExecutablePath", DOCTOR)
        self.assertLess(
            DOCTOR.index("RegisteredExecutableScope::CurrentUser"),
            DOCTOR.index("RegisteredExecutableScope::LocalMachine"),
        )
        self.assertIn("FileExists", DOCTOR)
        self.assertNotIn("RegGetValueW", DOCTOR)
        self.assertNotIn("HKEY_CURRENT_USER", DOCTOR)
        self.assertNotIn("HKEY_LOCAL_MACHINE", DOCTOR)
        self.assertNotIn("LSTATUS", DOCTOR)


if __name__ == "__main__":
    unittest.main()

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CMAKE = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
HEADER = (ROOT / "src" / "platform" / "system.h").read_text(encoding="utf-8")
WIN32 = (ROOT / "src" / "platform" / "system_win32.cpp").read_text(encoding="utf-8")
MACOS = (ROOT / "src" / "platform" / "system_macos.mm").read_text(encoding="utf-8")
DOCTOR = (ROOT / "src" / "ReaperBridgeDoctor.cpp").read_text(encoding="utf-8")


class ReaperBridgeDoctorEnvironmentContract(unittest.TestCase):
    def test_api_and_ctest_registration_are_native_free(self):
        for token in (
            "enum class EnvironmentStringStatus",
            "ValueTooLarge",
            "struct EnvironmentStringResult",
            "std::string valueUtf8",
            "EnvironmentStringResult ReadEnvironmentVariable(",
            "EnvironmentStringResult ExpandEnvironmentVariables(",
        ):
            self.assertIn(token, HEADER)
        for token in ("windows.h", "GetEnvironmentVariableW", "ExpandEnvironmentStringsW"):
            self.assertNotIn(token, HEADER)
        self.assertIn("TestReaperBridgeDoctorEnvironmentContract", CMAKE)

    def test_windows_environment_status_mapping_and_no_partial(self):
        for token in (
            "GetEnvironmentVariableW",
            "ExpandEnvironmentStringsW",
            "ERROR_ENVVAR_NOT_FOUND",
            "ERROR_MORE_DATA",
            "EnvironmentStringStatus::NotFound",
            "EnvironmentStringStatus::ValueTooLarge",
            "EnvironmentStringStatus::QueryFailed",
            "TextEncoding::WideToUtf8Strict",
        ):
            self.assertIn(token, WIN32)
        self.assertIn("wchar_t buffer[32768]", WIN32)

    def test_macos_uses_portable_read_and_unsupported_expand(self):
        self.assertIn("getenv", MACOS)
        self.assertIn("EnvironmentStringResult ExpandEnvironmentVariables", MACOS)
        self.assertIn("EnvironmentStringStatus::Unsupported", MACOS)

    def test_doctor_uses_pal_for_programfiles_and_userprofile_redaction(self):
        self.assertIn("Platform::ExpandEnvironmentVariables", DOCTOR)
        self.assertIn("Platform::ReadEnvironmentVariable", DOCTOR)
        self.assertIn('"%ProgramFiles%"', DOCTOR)
        self.assertIn('"USERPROFILE"', DOCTOR)
        for token in ("#include <windows.h>", "GetEnvironmentVariableW",
                      "ExpandEnvironmentStringsW", "DWORD"):
            self.assertNotIn(token, DOCTOR)
        self.assertIn("%USERPROFILE%", DOCTOR)


if __name__ == "__main__":
    unittest.main()

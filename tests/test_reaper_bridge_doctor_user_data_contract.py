import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = (ROOT / "src" / "platform" / "system.h").read_text(encoding="utf-8")
WIN32 = (ROOT / "src" / "platform" / "system_win32.cpp").read_text(encoding="utf-8")
MACOS = (ROOT / "src" / "platform" / "system_macos.mm").read_text(encoding="utf-8")
DOCTOR = (ROOT / "src" / "ReaperBridgeDoctor.cpp").read_text(encoding="utf-8")


class ReaperBridgeDoctorUserDataContract(unittest.TestCase):
    def test_public_result_api_is_native_free_and_explicit(self):
        for token in (
            "enum class UserDataDirectoryStatus",
            "Unsupported",
            "Success",
            "QueryFailed",
            "EncodingFailed",
            "struct UserDataDirectoryResult",
            "std::string pathUtf8",
            "unsigned long systemError",
            "UserDataDirectoryResult GetRoamingAppDataDirectory();",
        ):
            self.assertIn(token, HEADER)
        for token in ("windows.h", "DWORD", "NSBundle", "SHGetFolderPathW"):
            self.assertNotIn(token, HEADER)

    def test_win32_preserves_roaming_query_and_strict_result_mapping(self):
        self.assertIn("UserDataDirectoryResult GetRoamingAppDataDirectory()", WIN32)
        for token in (
            "wchar_t path[MAX_PATH]",
            "SHGetFolderPathW(\n        nullptr, CSIDL_APPDATA, nullptr",
            "SHGFP_TYPE_CURRENT",
            "!= S_OK",
            "UserDataDirectoryStatus::QueryFailed",
            "UserDataDirectoryStatus::EncodingFailed",
            "UserDataDirectoryStatus::Success",
            "WideToUtf8Strict",
            "CSIDL_APPDATA",
        ):
            self.assertIn(token, WIN32)
        self.assertIn("std::string GetAppDataDir()", WIN32)
        self.assertIn("EnsureLayout", WIN32)
        self.assertIn("HarkenUserData", WIN32)

    def test_macos_maps_application_support_query_without_child_path(self):
        start = MACOS.find("UserDataDirectoryResult GetRoamingAppDataDirectory()")
        end = MACOS.find("\n}\n", start) if start != -1 else -1
        source = MACOS[start:end if end != -1 else None] if start != -1 else ""
        for token in (
            "NSSearchPathForDirectoriesInDomains",
            "NSApplicationSupportDirectory",
            "NSUserDomainMask",
            "UserDataDirectoryStatus::QueryFailed",
            "UserDataDirectoryStatus::EncodingFailed",
            "UserDataDirectoryStatus::Success",
            "UTF8String",
        ):
            self.assertIn(token, source)
        self.assertNotIn("ArisAudio", source)
        self.assertNotIn("REAPER", source)

    def test_doctor_delegates_and_removes_shell_folder_native_path(self):
        self.assertIn('#include "platform/system.h"', DOCTOR)
        self.assertIn("Platform::GetRoamingAppDataDirectory()", DOCTOR)
        self.assertIn("UserDataDirectoryStatus::Success", DOCTOR)
        self.assertNotIn("SHGetFolderPathW", DOCTOR)
        self.assertNotIn("#include <shlobj.h>", DOCTOR)
        for token in (
        ):
            self.assertIn(token, DOCTOR)


if __name__ == "__main__":
    unittest.main()

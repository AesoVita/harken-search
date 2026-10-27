import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = (ROOT / "src" / "platform" / "system.h").read_text(encoding="utf-8")
WIN32 = (ROOT / "src" / "platform" / "system_win32.cpp").read_text(encoding="utf-8")
MACOS = (ROOT / "src" / "platform" / "system_macos.mm").read_text(encoding="utf-8")
DOCTOR = (ROOT / "src" / "ReaperBridgeDoctor.cpp").read_text(encoding="utf-8")


class ReaperBridgeDoctorProcessQueryContract(unittest.TestCase):
    def test_public_process_lookup_result_is_native_free(self):
        for token in (
            "enum class ProcessImageLookupStatus",
            "Unsupported",
            "Found",
            "NotFound",
            "InvalidArgument",
            "QueryFailed",
            "EncodingFailed",
            "struct ProcessImageLookupResult",
            "std::string executablePathUtf8",
            "unsigned long systemError",
            "ProcessImageLookupResult FindProcessByImageName(",
        ):
            self.assertIn(token, HEADER)
        for token in ("windows.h", "DWORD", "HANDLE", "PROCESSENTRY32W"):
            self.assertNotIn(token, HEADER)

    def test_win32_owns_snapshot_order_queries_and_error_mapping(self):
        self.assertIn("ProcessImageLookupResult FindProcessByImageName(", WIN32)
        start = WIN32.index("ProcessImageLookupResult FindProcessByImageName(")
        source = WIN32[start:]
        for token in (
            "Utf8ToWideStrict",
            "CreateToolhelp32Snapshot",
            "Process32FirstW",
            "Process32NextW",
            "CloseHandle(snapshot)",
            "OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION",
            "class ScopedProcessHandle",
            "~ScopedProcessHandle()",
            "CloseHandle(handle_)",
            "std::wstring path(32768, L'\\0')",
            "QueryFullProcessImageNameW",
            "ProcessImageLookupStatus::InvalidArgument",
            "ProcessImageLookupStatus::NotFound",
            "ProcessImageLookupStatus::QueryFailed",
            "ProcessImageLookupStatus::EncodingFailed",
            "ProcessImageLookupStatus::Found",
            "WideToUtf8Strict",
        ):
            self.assertIn(token, WIN32)
        self.assertLess(source.index("CloseHandle(snapshot)"),
                        source.index("OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION"))

    def test_macos_is_explicitly_unsupported_without_libproc(self):
        self.assertIn("ProcessImageLookupResult FindProcessByImageName(", MACOS)
        start = MACOS.find("ProcessImageLookupResult FindProcessByImageName(")
        source = MACOS[start:] if start != -1 else ""
        self.assertIn("ProcessImageLookupStatus::Unsupported", source)
        self.assertNotIn("libproc", source)

    def test_doctor_delegates_and_keeps_pe_native_scope(self):
        self.assertIn("Platform::FindProcessByImageName", DOCTOR)
        self.assertIn('"reaper.exe"', DOCTOR)
        self.assertIn("ProcessImageLookupStatus::Found", DOCTOR)
        for token in (
            "CreateToolhelp32Snapshot",
            "Process32FirstW",
            "Process32NextW",
            "QueryFullProcessImageNameW",
            "OpenProcess",
            "#include <tlhelp32.h>",
        ):
            self.assertNotIn(token, DOCTOR)
        for token in (
        ):
            self.assertIn(token, DOCTOR)


if __name__ == "__main__":
    unittest.main()

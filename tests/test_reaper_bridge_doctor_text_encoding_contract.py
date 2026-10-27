import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DOCTOR = (ROOT / "src" / "ReaperBridgeDoctor.cpp").read_text(encoding="utf-8")
HEADER_PATH = ROOT / "src" / "platform" / "text_encoding.h"
WIN32_PATH = ROOT / "src" / "platform" / "text_encoding_win32.cpp"
STD_PATH = ROOT / "src" / "platform" / "text_encoding_std.cpp"


class ReaperBridgeDoctorTextEncodingContract(unittest.TestCase):
    def test_public_strict_api_is_native_free(self):
        self.assertTrue(HEADER_PATH.is_file())
        header = HEADER_PATH.read_text(encoding="utf-8")
        self.assertIn("std::wstring Utf8ToWideStrict(std::string_view value);", header)
        self.assertIn("std::string WideToUtf8Strict(std::wstring_view value);", header)
        for token in ("windows.h", "MultiByteToWideChar", "WideCharToMultiByte"):
            self.assertNotIn(token, header)

    def test_win32_owns_exact_length_strict_conversion(self):
        self.assertTrue(WIN32_PATH.is_file())
        source = WIN32_PATH.read_text(encoding="utf-8")
        utf8_strict = source.split("Utf8ToWideStrict", 1)[1].split("WideToUtf8Strict", 1)[0]
        wide_tail = source.split("WideToUtf8Strict", 1)[1]
        wide_strict = wide_tail.split(
            "Utf8ToWidePermissive" if "Utf8ToWidePermissive" in wide_tail else "} // namespace",
            1,
        )[0]
        self.assertIn("MB_ERR_INVALID_CHARS", utf8_strict)
        self.assertIn("WC_ERR_INVALID_CHARS", wide_strict)
        self.assertIn("std::numeric_limits<int>::max()", utf8_strict)
        self.assertIn("std::numeric_limits<int>::max()", wide_strict)
        self.assertEqual(utf8_strict.count("MultiByteToWideChar("), 2)
        self.assertEqual(wide_strict.count("WideCharToMultiByte("), 2)
        self.assertIn("converted != expected", utf8_strict)
        self.assertIn("converted != expected", wide_strict)

    def test_portable_implementation_has_no_win32_dependency(self):
        self.assertTrue(STD_PATH.is_file())
        source = STD_PATH.read_text(encoding="utf-8")
        self.assertNotIn("windows.h", source)
        self.assertNotIn("MultiByteToWideChar", source)
        self.assertNotIn("WideCharToMultiByte", source)

    def test_doctor_delegates_without_changing_other_native_work(self):
        self.assertIn('#include "platform/text_encoding.h"', DOCTOR)
        self.assertIn("Platform::TextEncoding::Utf8ToWideStrict(value)", DOCTOR)
        self.assertIn("Platform::TextEncoding::WideToUtf8Strict(value)", DOCTOR)
        self.assertNotIn("MultiByteToWideChar", DOCTOR)
        self.assertNotIn("WideCharToMultiByte", DOCTOR)


if __name__ == "__main__":
    unittest.main()

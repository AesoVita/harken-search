import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = (ROOT / "src" / "platform" / "text_encoding.h").read_text(encoding="utf-8")
ASCII_IMPL = ROOT / "src" / "platform" / "text_encoding_ascii.cpp"
DOCTOR = (ROOT / "src" / "ReaperBridgeDoctor.cpp").read_text(encoding="utf-8")


class ReaperBridgeDoctorAsciiCompareContract(unittest.TestCase):
    def test_native_free_ascii_compare_api_and_implementation(self):
        self.assertIn(
            "bool EqualsAsciiCaseInsensitive(std::string_view left, std::string_view right);",
            HEADER,
        )
        self.assertTrue(ASCII_IMPL.is_file())
        source = ASCII_IMPL.read_text(encoding="utf-8")
        self.assertIn("unsigned char", source)
        self.assertIn("'A'", source)
        self.assertIn("'Z'", source)
        self.assertNotIn("locale", source.lower())

    def test_doctor_uses_utf8_basename_helper_without_wcsicmp(self):
        self.assertIn('#include "platform/text_encoding.h"', DOCTOR)
        self.assertIn("Platform::TextEncoding::EqualsAsciiCaseInsensitive", DOCTOR)
        self.assertNotIn("_wcsicmp", DOCTOR)
        self.assertIn('"reaper.exe"', DOCTOR)


if __name__ == "__main__":
    unittest.main()

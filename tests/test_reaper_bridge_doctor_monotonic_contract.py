import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DOCTOR = (ROOT / "src" / "ReaperBridgeDoctor.cpp").read_text(encoding="utf-8")
SYSTEM_HEADER = (ROOT / "src" / "platform" / "system.h").read_text(encoding="utf-8")
SYSTEM_WIN32 = (ROOT / "src" / "platform" / "system_win32.cpp").read_text(encoding="utf-8")
SYSTEM_MACOS = (ROOT / "src" / "platform" / "system_macos.mm").read_text(encoding="utf-8")


class ReaperBridgeDoctorMonotonicContract(unittest.TestCase):
    def test_transaction_names_use_monotonic_platform_time(self):
        self.assertIn('#include "platform/system.h"', DOCTOR)
        self.assertIn("Platform::MonotonicMilliseconds()", DOCTOR)
        self.assertNotIn("GetTickCount64", DOCTOR)

    def test_install_transaction_native_scope_remains_unchanged(self):
        for token in (
        ):
            self.assertIn(token, DOCTOR)

    def test_current_process_id_uses_cross_platform_system_pal(self):
        self.assertIn("uint64_t CurrentProcessId();", SYSTEM_HEADER)
        self.assertIn("uint64_t CurrentProcessId()", SYSTEM_WIN32)
        self.assertIn("GetCurrentProcessId()", SYSTEM_WIN32)
        self.assertIn("uint64_t CurrentProcessId()", SYSTEM_MACOS)
        self.assertIn("getpid()", SYSTEM_MACOS)
        self.assertIn("Platform::CurrentProcessId()", DOCTOR)
        self.assertNotIn("GetCurrentProcessId()", DOCTOR)


if __name__ == "__main__":
    unittest.main()

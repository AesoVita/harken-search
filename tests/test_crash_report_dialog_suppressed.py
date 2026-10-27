"""RQ-20260917-03: startup must not open the abnormal-exit report dialog.

The crash sentinel, submit path, and ledger copy stay. Only the
startup popup is disconnected. Keys that existed solely for that popup
are exempted, not deleted.
"""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import generate_localization


PARKED_CRASH_DIALOG_KEYS = (
    "Crash_DlgTitle",
    "Main_CrashTitleText",
    "Main_CrashReportNote",
    "Crash_SendReport",
    "Fb_CrashReportTitle",
    "Main_AttachDiagLog500",
    "Main_DismissBtn",
)


class CrashReportDialogSuppressedTests(unittest.TestCase):
    def test_startup_does_not_open_the_abnormal_exit_popup(self) -> None:
        main = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
        start = main.index("// F-035: Crash Recovery Dialog")
        end = main.index("// F-035: Beta Feedback Dialog", start)
        body = main[start:end]

        self.assertNotIn("OpenPopup", body)
        self.assertNotIn("BeginTransientScrollbarPopupModal", body)
        self.assertNotIn("showCrashReportDialog = true", body)
        self.assertIn("crashDetectedOnStartup = false", body)

    def test_crash_detection_and_submit_channel_stay(self) -> None:
        windows = (ROOT / "src" / "WindowsMain.cpp").read_text(encoding="utf-8")
        sentinel = windows.split("// F-035: Crash sentinel", 1)[1].split(
            "// Settings loading may replace the path", 1
        )[0]
        self.assertIn("state.crashDetectedOnStartup = true", sentinel)
        self.assertIn("aris_sentinel.lock", sentinel)

        feedback = (ROOT / "src" / "CueviFeedbackClient.cpp").read_text(encoding="utf-8")
        self.assertIn("CrashRecovery", feedback)
        self.assertIn("Submit(", feedback)

    def test_parked_dialog_copy_is_exempted_not_deleted(self) -> None:
        ledger = generate_localization.load_ledger(generate_localization.DEFAULT_LEDGER)
        ids = {entry["id"] for entry in ledger["entries"]}
        for key in PARKED_CRASH_DIALOG_KEYS:
            self.assertIn(key, ids)
            self.assertIn(key, generate_localization.PROTECTED_LEDGER_IDS)


if __name__ == "__main__":
    unittest.main()

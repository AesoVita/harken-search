import re
import unittest
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
SOURCE_PATH = ROOT / "tests/test_stage6_scan_terminal_throughput_probe.cpp"
CMAKE_PATH = ROOT / "cmake/harken_search_behavior_tests.cmake"
SOURCE = SOURCE_PATH.read_text(encoding="utf-8") if SOURCE_PATH.is_file() else ""
CMAKE = CMAKE_PATH.read_text(encoding="utf-8") if CMAKE_PATH.is_file() else ""
class ScanTerminalThroughputProbeContractTests(unittest.TestCase):
    def test_focused_target_and_public_boundary(self):
        self.assertTrue(SOURCE_PATH.is_file())
        self.assertRegex(CMAKE, r"(?s)add_executable\(TestStage6ScanTerminalThroughputProbe\s+EXCLUDE_FROM_ALL.*?test_stage6_scan_terminal_throughput_probe\.cpp")
        self.assertIn("add_test(NAME TestStage6ScanTerminalThroughputProbe", CMAKE)
        for token in ("Database", "resetScanForegroundCompletionSession", "onDbOpened", "BackfillWorkerPool", "scanDirectoryEx", "getScanForegroundCompletionCounts", "countBackfillPending", "writeTinyCoverImage", "0x89,0x50,0x4E,0x47", "cover.jpg", "getFileByPath", "folderCoverKey"):
            self.assertIn(token, SOURCE)
        self.assertIn("completeImport=false", SOURCE)
        self.assertIn("enqueueExtraction=true", SOURCE)
        self.assertIn("counts.complete()", SOURCE)
        self.assertIn("counts.failureTerminal != kFiles", SOURCE)
    def test_fixed_fixture_workers_deadlines_and_json_contract(self):
        for token in ("pcm_s16_wav", "sample_rate", "channels", "frames_per_file", "total", "added", "updated", "skipped", "ingested", "errors", "ingest_duration_ms", "pending_files", "mini_terminal", "cover_terminal", "metadata_terminal", "mini_unresolved", "cover_unresolved", "metadata_unresolved", "non_success_terminal", "resolved_cover_files", "elapsed_ms", "files_per_second", "value_files_per_second", "environment_gate", "measured"):
            self.assertIn(token, SOURCE)
        for declaration in (r"kFiles\s*=\s*100", r"kScanThreads\s*=\s*2", r"kBackfillThreads\s*=\s*2", r"kTerminalTimeoutSeconds\s*=\s*30"):
            self.assertRegex(SOURCE, declaration)
        self.assertRegex(CMAKE, r"(?s)set_tests_properties\(TestStage6ScanTerminalThroughputProbe.*?TIMEOUT\s+60")
        self.assertIn('std::cout << result.json', SOURCE)
        self.assertRegex(SOURCE, r'(?s)files_per_second\\":.*filesPerSecond.*value_files_per_second\\":.*filesPerSecond')
        for forbidden in ("failure_terminal", "kProbeDeadlineSeconds", "wholeDeadline", "sqlite3_", ".db()", "AppState", "ImGui", "main.cpp", "WindowsMain.cpp"):
            self.assertNotIn(forbidden, SOURCE)
    def test_exit_and_cleanup_contract(self):
        self.assertRegex(SOURCE, r"(?s)worker\.stop\(\).*queue\.shutdown\(\).*db\.close\(\).*fs::remove_all")
        self.assertRegex(SOURCE, r"result\.code\s*=\s*2")
        self.assertRegex(SOURCE, r"code\s*=\s*1")
        self.assertNotRegex(SOURCE, r"reason\s*=\s*e\.what\(\)")
        self.assertRegex(SOURCE, r"terminalDeadline\s*=\s*Clock::now\(\)\s*\+\s*std::chrono::seconds\(kTerminalTimeoutSeconds\)")
if __name__ == "__main__":
    unittest.main()

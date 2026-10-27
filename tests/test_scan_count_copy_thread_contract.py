import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
WINDOWS_MAIN = (ROOT / "src/WindowsMain.cpp").read_text(encoding="utf-8")
CMAKE = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")


def scan_thread_lambda(source: str, owner_marker: str) -> str:
    owner = source.index(owner_marker)
    thread = source.index("state.scanThread = std::thread", owner)
    catch = source.index("} catch (...)", thread)
    return source[thread:catch]


class ScanCountCopyThreadContract(unittest.TestCase):
    def test_contract_is_registered(self):
        self.assertIn("TestScanCountCopyThreadContract", CMAKE)
        self.assertIn("tests/test_scan_count_copy_thread_contract.py", CMAKE)

    def test_ui_entry_counting_copy_stays_on_ui_thread(self):
        start_scan = MAIN[MAIN.index("auto startScanForFolders") : MAIN.index("state.scanThread = std::thread", MAIN.index("auto startScanForFolders"))]
        begin_scan = WINDOWS_MAIN[WINDOWS_MAIN.index("static bool BeginFolderScan") : WINDOWS_MAIN.index("state.scanThread = std::thread", WINDOWS_MAIN.index("static bool BeginFolderScan"))]
        self.assertIn("state.setScanFile(L(Wf_CountingFiles));", start_scan)
        self.assertIn("state.setScanFile(L(Wf_CountingFiles));", begin_scan)

    def test_start_scan_thread_does_not_translate_counting_copy(self):
        lambda_body = scan_thread_lambda(MAIN, "auto startScanForFolders")
        self.assertIn("for (size_t fi = 0; fi < foldersToScan.size(); ++fi)", lambda_body)
        self.assertNotIn("L(Wf_CountingFiles)", lambda_body)
        self.assertIn("LStr::Wf_CountingFiles", lambda_body)
        self.assertIn("setScanFileKeyPrefixed", lambda_body)
        self.assertIn('u8"["', lambda_body)

    def test_begin_folder_scan_thread_does_not_translate_counting_copy(self):
        lambda_body = scan_thread_lambda(WINDOWS_MAIN, "static bool BeginFolderScan")
        self.assertIn("for (size_t fi = 0; fi < foldersToScan.size(); ++fi)", lambda_body)
        self.assertNotIn("L(Wf_CountingFiles)", lambda_body)
        self.assertIn("LStr::Wf_CountingFiles", lambda_body)
        self.assertIn("setScanFileKeyPrefixed", lambda_body)
        self.assertIn('u8"["', lambda_body)


if __name__ == "__main__":
    unittest.main()

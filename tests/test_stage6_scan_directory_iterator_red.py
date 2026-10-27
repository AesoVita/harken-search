import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class ScanDirectoryIteratorRedContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        source = (ROOT / "src" / "Database.cpp").read_text(encoding="utf-8")
        cls.source = source
        cls.body = source.split("auto scanOneDirectory =", 1)[1].split(
            "// Begin explicit-stack traversal", 1
        )[0]

    def test_scan_directory_uses_filesystem_directory_iterator_pal(self) -> None:
        body = self.body
        self.assertIn('#include "platform/filesystem.h"', self.source)
        self.assertIn("Platform::FileSystem::DirectoryIterator", body)
        self.assertIn("Platform::FileSystem::DirectoryReadStatus::Entry", body)
        self.assertIn("Platform::FileSystem::DirectoryReadStatus::End", body)
        self.assertIn("Platform::FileSystem::DirectoryReadStatus::Error", body)
        self.assertNotIn("fs::directory_iterator", body)

    def test_scan_directory_preserves_cancellation_stack_and_audio_dispatch(self) -> None:
        body = self.body
        self.assertIn("isCancelled()", body)
        self.assertIn("fatalScanError", body)
        self.assertIn("dirStack.push_back", body)
        self.assertIn("handleAudioPath", body)
        self.assertIn("processWorkBatch(work)", body)
        self.assertIn("iterator.Next", body)

    def test_open_failure_keeps_original_silent_skip_semantics(self) -> None:
        body = self.body
        open_branch = body.split("if (!iterator.Open", 1)[1].split("return;", 1)[0]
        self.assertNotIn("stats.addError", open_branch)
        self.assertNotIn("m_lastError", open_branch)


if __name__ == "__main__":
    unittest.main()

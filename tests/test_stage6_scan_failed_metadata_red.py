import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class ScanFailedMetadataRedContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        source = (ROOT / "src" / "Database.cpp").read_text(encoding="utf-8")
        cls.source = source
        cls.body = source.split("auto scanOneDirectory =", 1)[1].split(
            "// Begin explicit-stack traversal", 1
        )[0]

    def test_exception_work_items_use_metadata_pal(self) -> None:
        body = self.body
        self.assertIn('#include "platform/filesystem.h"', self.source)
        self.assertEqual(2, body.count("Platform::FileSystem::ProbeRegularFile"))
        self.assertEqual(2, body.count("Platform::FileSystem::LastWriteTimeUnixSeconds"))
        self.assertNotIn("fs::file_size", body)
        self.assertNotIn("fs::last_write_time", body)

    def test_exception_work_items_keep_zero_fallback_and_terminal_errors(self) -> None:
        body = self.body
        self.assertEqual(2, body.count("failed.scanTerminalFailure = true"))
        self.assertGreaterEqual(body.count("failed.fileSize"), 2)
        self.assertGreaterEqual(body.count("failed.fileMtime"), 2)
        self.assertGreaterEqual(body.count("catch (...)"), 2)
        self.assertIn("unknown error", body)


if __name__ == "__main__":
    unittest.main()

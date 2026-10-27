import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class ScanMetadataSnapshotRedContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        source = (ROOT / "src" / "Database.cpp").read_text(encoding="utf-8")
        cls.source = source
        cls.body = source.split("auto handleAudioPath =", 1)[1].split(
            "// Files below kMinAudioFileSize", 1
        )[0]

    def test_scan_snapshot_uses_filesystem_metadata_pal(self) -> None:
        body = self.body
        self.assertIn('#include "platform/filesystem.h"', self.source)
        self.assertIn("Platform::FileSystem::ProbePath", body)
        self.assertIn("Platform::FileSystem::ProbeRegularFile", body)
        self.assertIn("Platform::FileSystem::LastWriteTimeUnixSeconds", body)
        self.assertNotIn("fs::exists", body)
        self.assertNotIn("fs::file_size", body)
        self.assertNotIn("fs::last_write_time", body)

    def test_scan_snapshot_preserves_long_path_fallback_and_unavailable_values(self) -> None:
        body = self.body
        self.assertIn("MakeLongPath(rawPath)", body)
        self.assertIn("sizeAvailable", body)
        self.assertIn(": 0", body)
        self.assertIn("wi.fileMtime", body)
        self.assertLess(
            body.index("Platform::FileSystem::ProbePath"),
            body.index("Platform::FileSystem::ProbeRegularFile"),
        )


if __name__ == "__main__":
    unittest.main()

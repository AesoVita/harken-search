from __future__ import annotations

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class DatabaseScanDirectoryProbeContract(unittest.TestCase):
    def test_scan_directory_entry_uses_filesystem_directory_probe_pal(self) -> None:
        database = (ROOT / "src" / "Database.cpp").read_text(encoding="utf-8")
        body = database.split("ScanStats Database::scanDirectoryEx(", 1)[1].split(
            "const bool sessionOwnedByThisCall", 1
        )[0]

        self.assertEqual(2, body.count("Platform::FileSystem::ProbePath("))
        self.assertEqual(2, body.count("Platform::FileSystem::PathStatus::Present"))
        self.assertEqual(2, body.count("Platform::FileSystem::IsDirectory("))
        self.assertNotIn("fs::exists(", body)
        self.assertNotIn("fs::is_directory(", body)


if __name__ == "__main__":
    unittest.main()

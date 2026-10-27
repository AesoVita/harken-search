from __future__ import annotations

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DATABASE = (ROOT / "src/Database.cpp").read_text(encoding="utf-8")


class DatabaseRemainingFilesystemContract(unittest.TestCase):
    def test_remove_folder_disk_cleanup_uses_filesystem_pal(self) -> None:
        body = DATABASE.split("int Database::removeFolderSubtreeFromDb(", 1)[1].split(
            'DIAG_ACTION_FMT("Database",\n                    "RemoveFolderSubtree', 1
        )[0]
        self.assertIn("Platform::FileSystem::ProbePath", body)
        self.assertIn("Platform::FileSystem::ProbeRegularFile", body)
        self.assertIn("Platform::FileSystem::RemoveFile", body)
        self.assertNotIn("fs::is_regular_file", body)
        self.assertNotIn("fs::remove(", body)

    def test_watcher_mtime_uses_filesystem_pal(self) -> None:
        body = DATABASE.split("void Database::handleFileSystemEvent(", 1)[1].split(
            "// Re-read metadata tags.", 1
        )[0]
        self.assertIn("Platform::FileSystem::LastWriteTimeUnixSeconds(filePath)", body)
        self.assertNotIn("fs::last_write_time", body)

    def test_package_cleanup_routes_recursive_cache_removal_through_pal(self) -> None:
        body = DATABASE.split("bool Database::DestroyDatabasePackage(", 1)[1]
        self.assertIn("Platform::FileSystem::RemoveFile(PathToUtf8(parentDir))", body)
        self.assertNotIn("fs::remove(parentDir", body)
        self.assertIn("Platform::FileSystem::RemoveAll(PathToUtf8(cacheDir)", body)
        self.assertNotIn("fs::remove_all(cacheDir", body)


if __name__ == "__main__":
    unittest.main()

import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class DatabaseOpenLifecycleRedContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        source = (ROOT / "src" / "Database.cpp").read_text(encoding="utf-8")
        cls.open_body = source.split("bool Database::open(", 1)[1].split(
            "// Register search-normalization functions", 1
        )[0]
        cls.cleanup = cls.open_body.split("auto cleanupOnFail", 1)[1].split(
            "if (result != SQLITE_OK)", 1
        )[0]

    def test_open_distinguishes_missing_present_and_error(self) -> None:
        body = self.open_body
        self.assertIn("Platform::FileSystem::ProbePath(dbPath", body)
        self.assertIn("Platform::FileSystem::PathStatus::Missing", body)
        self.assertIn("Platform::FileSystem::PathStatus::Present", body)
        self.assertIn("Platform::FileSystem::PathStatus::Error", body)
        self.assertIn("isNewFile", body)
        self.assertNotIn("isNewFile = !fs::exists", body)

    def test_parent_directory_error_stops_before_sqlite_open(self) -> None:
        body = self.open_body
        self.assertIn("Platform::FileSystem::EnsureDirectoryExists", body)
        ensure = body.index("Platform::FileSystem::EnsureDirectoryExists")
        sqlite_open = body.index("sqlite3_open")
        self.assertLess(ensure, sqlite_open)
        between = body[ensure:sqlite_open]
        self.assertIn("return false", between)
        self.assertNotIn("fs::create_directories", body[:sqlite_open])

    def test_only_missing_database_is_eligible_for_failure_cleanup(self) -> None:
        body = self.open_body
        self.assertIn("Platform::FileSystem::PathStatus::Missing", body)
        missing = body.index("Platform::FileSystem::PathStatus::Missing")
        is_new = body.index("isNewFile")
        self.assertLess(missing, is_new)
        self.assertIn("if (isNewFile)", self.cleanup)
        guard = self.cleanup.index("if (isNewFile)")
        first_remove = self.cleanup.index("Platform::FileSystem::RemoveFile")
        self.assertLess(guard, first_remove)

    def test_missing_database_cleanup_removes_main_and_both_sidecars(self) -> None:
        cleanup = self.cleanup
        self.assertEqual(3, cleanup.count("Platform::FileSystem::RemoveFile("))
        self.assertIn("Platform::FileSystem::RemoveFile(dbPath", cleanup)
        self.assertIn("getBlobsDbPath(dbPath)", cleanup)
        self.assertIn("getWaveformsDbPath(dbPath)", cleanup)
        self.assertNotIn("fs::remove", cleanup)


if __name__ == "__main__":
    unittest.main()

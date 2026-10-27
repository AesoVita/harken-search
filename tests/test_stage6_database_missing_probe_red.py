import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
DATABASE = (ROOT / "src" / "Database.cpp").read_text(encoding="utf-8")


class DatabaseMissingProbeContract(unittest.TestCase):
    def test_cleanup_only_classifies_explicitly_missing_paths_as_missing(self):
        full_cleanup = DATABASE.split("int Database::removeMissingFilesDatabaseWide(", 1)[1].split(
            "// Map column names once", 1
        )[0]
        scan_cleanup = DATABASE.split("if (removeMissing && !wasCancelled", 1)[1].split(
            "int uniqueCoverChanged", 1
        )[0]

        for cleanup in (full_cleanup, scan_cleanup):
            self.assertIn("Platform::FileSystem::ProbePath(fp)", cleanup)
            self.assertIn("Platform::FileSystem::PathStatus::Missing", cleanup)
            self.assertNotIn("fs::exists", cleanup)
            self.assertNotIn("Platform::FileSystem::Exists", cleanup)


if __name__ == "__main__":
    unittest.main()

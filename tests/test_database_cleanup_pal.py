import pathlib
import unittest


class DatabaseCleanupPalContractTests(unittest.TestCase):
    def test_destroy_package_delegates_primary_retry_to_platform_pal(self):
        source = (pathlib.Path(__file__).resolve().parents[1] / "src" / "Database.cpp").read_text(
            encoding="utf-8"
        )
        self.assertNotIn("auto tryRemoveWithRetry =", source)
        body = source.split("bool Database::DestroyDatabasePackage", 1)[1].split(
            "// getCoverFacets is implemented above.", 1
        )[0]
        self.assertEqual(body.count("Platform::FileSystem::RemoveFile("), 3)
        self.assertEqual(source.count("removePackageFile("), 3)

        header = (
            pathlib.Path(__file__).resolve().parents[1] / "src" / "platform" / "filesystem.h"
        ).read_text(encoding="utf-8")
        self.assertIn(
            "bool RemoveFile(const std::string& pathUtf8,",
            header,
        )


if __name__ == "__main__":
    unittest.main()

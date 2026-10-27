import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class DatabaseSidecarPrepareRedContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        source = (ROOT / "src" / "Database.cpp").read_text(encoding="utf-8")
        cls.source = source
        cls.body = source.split("bool Database::attachSidecarDatabases", 1)[1].split(
            "bool Database::createSidecarTables", 1
        )[0]

    def test_sidecar_prepare_uses_filesystem_pal_for_parent_and_states(self) -> None:
        body = self.body
        self.assertIn('#include "platform/filesystem.h"', self.source)
        self.assertIn("Platform::FileSystem::EnsureDirectoryExists", body)
        self.assertIn("Platform::FileSystem::ProbePath", body)
        for status in ("Missing", "Present", "Error"):
            self.assertIn(f"Platform::FileSystem::PathStatus::{status}", body)
        self.assertNotIn("fs::create_directories", body)
        self.assertNotIn("fs::exists", body)

    def test_missing_sidecars_are_created_but_present_sidecars_are_reused(self) -> None:
        body = self.body.split("} else try {", 1)[1]
        self.assertIn("Platform::FileSystem::PathStatus::Missing", body)
        self.assertIn("Platform::FileSystem::PathStatus::Present", body)
        missing = body.index("Platform::FileSystem::PathStatus::Missing")
        present = body.index("Platform::FileSystem::PathStatus::Present")
        self.assertLess(missing, present)
        self.assertIn("sqlite3_open", body[missing:])
        self.assertIn("blobsPath", body)
        self.assertIn("waveformsPath", body)

    def test_read_only_attach_keeps_missing_sidecars_in_memory(self) -> None:
        read_only = self.body.split("if (readOnly) {", 1)[1].split("} else try {", 1)[0]
        self.assertIn("PRAGMA query_only=ON", read_only)
        self.assertNotIn("sqlite3_open", read_only)
        self.assertIn('readOnly && blobsMissing', self.body)
        self.assertIn('readOnly && waveformsMissing', self.body)

    def test_sidecar_prepare_errors_preserve_formal_attach_fallback(self) -> None:
        body = self.body
        self.assertIn("Platform::FileSystem::PathStatus::Error", body)
        error = body.index("Platform::FileSystem::PathStatus::Error")
        attach = body.index("ATTACH DATABASE")
        self.assertLess(error, attach)
        preparation = body[:attach]
        self.assertIn("probeError", preparation)
        isolation_guard, filesystem_prepare = preparation.split("try {", 1)
        self.assertIn("PreviewIsolationAllowsDataPathUtf8(m_blobsDbPath)", isolation_guard)
        self.assertIn("PreviewIsolationAllowsDataPathUtf8(m_waveformsDbPath)", isolation_guard)
        self.assertIn("return false", isolation_guard)
        self.assertNotIn("return false", filesystem_prepare)

    def test_entire_preparation_is_exception_contained_before_attach(self) -> None:
        body = self.body
        preparation = body.split("m_waveformsDbPath =", 1)[1].split(
            "// Attach the artwork-thumbnail sidecar", 1
        )[0]
        self.assertIn("try {", preparation)
        self.assertIn("catch (const std::exception&", preparation)
        self.assertIn("catch (...)" , preparation)
        self.assertLess(preparation.index("try {"), preparation.index("Utf8ToPath"))
        self.assertLess(preparation.index("sqlite3_open"), preparation.index("catch"))
        catch_tail = preparation[preparation.index("catch"):]
        self.assertNotIn("return false", catch_tail)


if __name__ == "__main__":
    unittest.main()

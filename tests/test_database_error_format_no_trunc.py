import pathlib
import re
import unittest


class DatabaseErrorFormatNoTruncContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = (
            pathlib.Path(__file__).resolve().parents[1] / "src" / "Database.cpp"
        ).read_text(encoding="utf-8")

    def test_named_diagnostic_buffers_are_not_fixed_size(self):
        self.assertIsNone(
            re.search(r"char\s+dbE[1-9]\s*\[\s*512\s*\]", self.source),
            "Database.cpp must not format sqlite/path errors into char dbE1..dbE9[512]",
        )
        self.assertIsNone(
            re.search(r"char\s+b10\s*\[\s*768\s*\]", self.source),
            "Database.cpp must not format FTS5 probe errors into char b10[768]",
        )
        self.assertIsNone(
            re.search(r"char\s+dbE11\s*\[\s*1024\s*\]", self.source),
            "Database.cpp must not format delete-locked errors into char dbE11[1024]",
        )
        self.assertIsNone(
            re.search(r"char\s+dbE\d+\s*\[\s*\d+\s*\]", self.source),
            "Database.cpp must not keep any named char dbE*[N] diagnostic stack buffer",
        )
        self.assertIsNone(
            re.search(r"char\s+b10\s*\[\s*\d+\s*\]", self.source),
            "Database.cpp must not keep char b10[N] as a diagnostic stack buffer",
        )

    def test_error_format_uses_dynamic_length_and_keeps_l_keys(self):
        self.assertRegex(
            self.source,
            r"snprintf\s*\(\s*nullptr\s*,\s*0\s*,",
            "long sqlite/path errors must size the diagnostic with snprintf(nullptr, 0) then allocate",
        )
        for key in (
            "Db_Fts5DisabledFmt",
            "Db_FtsUpgradeFailFmt",
            "Db_FtsCreateFailFmt",
            "Db_FtsVocabFailFmt",
            "Db_FtsTriggerFailFmt",
            "Db_FtsRebuildFailFmt",
            "Db_TrigramUpgradeFailFmt",
            "Db_TrigramCreateFailFmt",
            "Db_TrigramTriggerFailFmt",
            "Db_TrigramRebuildFailFmt",
            "Db_DeleteLockedFmt",
        ):
            self.assertIn(
                f"L({key})",
                self.source,
                f"must keep existing L({key}) + %s format, not replace it with LedgerText",
            )


if __name__ == "__main__":
    unittest.main()

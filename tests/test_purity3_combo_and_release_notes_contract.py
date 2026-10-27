import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")


class Purity3ComboAndReleaseNotesContract(unittest.TestCase):
    def test_occurrence_combo_keeps_explicit_nul_separators(self):
        start = MAIN.index('if (needsOccurrenceSelector)')
        block = MAIN[start:MAIN.index("ImGui::Combo(\"##RenameDeleteToEndOccurrence\"", start) + 80]
        self.assertNotRegex(
            block,
            r'\+\s*"\\0"',
            "C-string \"\\0\" is empty; Combo items must be joined with explicit NULs",
        )
        self.assertIn("occurrenceItems.push_back('\\0')", block)
        self.assertIn("occurrenceItems += L(Occur_LastHit)", block)
        self.assertGreaterEqual(block.count("push_back('\\0')"), 3)

    def test_empty_release_notes_do_not_cache_translated_copy(self):
        start = MAIN.index("static void LoadReleaseNotesFromExeDir(bool forceReload)")
        load_fn = MAIN[start:MAIN.index("std::string ControlUpdateNoticeForQa", start)]
        self.assertNotIn("L(ReleaseNotes_Empty)", load_fn)
        self.assertNotIn("L(Update_InlineNoNotes)", load_fn)
        notice = (ROOT / "src" / "update" / "HarkenSearchUpdateNoticeUi.cpp").read_text(encoding="utf-8")
        start_render = notice.index("const bool failed = candidate")
        render = notice[start_render:notice.index("ImGui::TextWrapped", start_render) + 80]
        self.assertIn("L(Update_InlineNoNotes)", render)
        self.assertNotIn("notes.message =", render)


if __name__ == "__main__":
    unittest.main()

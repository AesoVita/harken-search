import pathlib
import json
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
LOCALIZED = {
    entry["id"]: entry
    for entry in json.loads(
        (ROOT / "locale" / "harken.ui.strings.json").read_text(encoding="utf-8")
    )["entries"]
}


class SearchPrivacySourceContractTests(unittest.TestCase):
    def test_ordinary_tabs_do_not_persist_or_restore_search_terms(self):
        main = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")

        self.assertNotIn('kv.find(prefix + "searchBuffer")', main)
        self.assertNotIn('items.push_back({prefix + "searchBuffer"', main)
        self.assertIn('kv.find(tp + "searchBuffer")', main)
        self.assertIn('items.push_back({tp + "searchBuffer"', main)

    def test_search_terms_do_not_enter_diagnostics_or_user_action_log(self):
        main = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
        refresh_diagnostics = (ROOT / "src" / "AsyncSearchRefreshDiagnosticsStateAdapter.cpp").read_text(encoding="utf-8")

        self.assertNotIn("UA_LOG_FMT(UACategory::Search", main)
        self.assertIn('DIAG_ACTION("Search", "KeywordSearch")', main)
        self.assertNotIn('DIAG_ACTION_FMT("Search", "KeywordSearch: %s"', main)
        self.assertNotIn('"No results for query: %s"', refresh_diagnostics)
        self.assertIn('"No results for query"', refresh_diagnostics)

    def test_diagnostic_attachments_start_closed_and_remain_explicit(self):
        app_state = (ROOT / "src" / "AppState.h").read_text(encoding="utf-8")
        main = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")

        self.assertIn("bool feedbackAttachLog = false", app_state)
        self.assertIn("bool crashReportAttachLog = false", app_state)
        self.assertIn("F-035: Crash Recovery Dialog is parked", main)
        self.assertNotIn("&state.crashReportAttachLog", main)
        self.assertIn('ImGui::Checkbox(L(Main_AttachDiagLog200), &state.feedbackAttachLog)', main)
        self.assertEqual(LOCALIZED["Main_AttachDiagLog200"]["zh"], "附上诊断日志（最近 200 行）")
        self.assertIn('bool attachLog = state.feedbackAttachLog;', main)
        self.assertIn('recentLog = attachLog\n                ? DiagnosticsLogger::instance().getRecentFilteredLines(200) : "";', main)


if __name__ == "__main__":
    unittest.main()

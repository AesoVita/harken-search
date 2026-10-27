import json
import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class SearchHitColumnIdentitySourceContractTests(unittest.TestCase):
    def test_header_resolves_by_query_field_not_snippet(self):
        header_path = ROOT / "src" / "SearchHitColumnIdentity.h"
        self.assertTrue(header_path.is_file(), "SearchHitColumnIdentity.h must exist")
        header = header_path.read_text(encoding="utf-8")
        self.assertIn("ResolveSearchHitColumnIndex", header)
        self.assertIn("ShouldRevealSearchHitColumn", header)
        self.assertIn("BuildSearchQueryChips", header)
        self.assertIn("chip.field", header)
        self.assertNotIn("matchSnippet", header)
        self.assertNotIn("searchResultsGen_", header)
        self.assertIn("Artist = 10", header)
        self.assertIn("Category = 9", header)
        self.assertIn("Album = 11", header)
        self.assertIn("FilePath = 16", header)

    def test_highlight_spans_header_includes_iterator(self):
        header = (ROOT / "src" / "ResultRowHighlightSpans.h").read_text(encoding="utf-8")
        self.assertIn("#include <iterator>", header)

    def test_cmake_and_harness_build_the_identity_and_span_tests(self):
        cmake = (ROOT / "cmake" / "harken_search_behavior_tests.cmake").read_text(encoding="utf-8")
        self.assertIn("add_executable(TestSearchHitColumnIdentity", cmake)
        self.assertIn("tests/test_search_hit_column_identity.cpp", cmake)
        self.assertIn("aura_set_test_labels(TestSearchHitColumnIdentity db)", cmake)
        self.assertIn("aura_set_test_labels(TestResultRowHighlightSpans db)", cmake)

        config = json.loads((ROOT / ".openclaw" / "harness" / "harness_config.json").read_text(encoding="utf-8"))
        targets = config.get("dbBuildTargets", [])
        self.assertIn("TestResultRowHighlightSpans", targets)
        self.assertIn("TestSearchHitColumnIdentity", targets)

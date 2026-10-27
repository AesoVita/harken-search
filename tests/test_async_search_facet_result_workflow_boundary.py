from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/AsyncSearchFacetResultWorkflow.h"
SOURCE = ROOT / "src/AsyncSearchFacetResultWorkflow.cpp"


def test_async_search_facet_result_workflow_owns_two_result_branches() -> None:
    assert HEADER.is_file(), f"missing header: {HEADER}"
    assert SOURCE.is_file(), f"missing source: {SOURCE}"
    header = HEADER.read_text(encoding="utf-8")
    source = SOURCE.read_text(encoding="utf-8")
    main = (ROOT / "src/AsyncSearchResultApplicationAdapter.cpp").read_text(
        encoding="utf-8"
    )
    adapter = (ROOT / "src/AsyncSearchFacetResultStateAdapter.cpp").read_text(
        encoding="utf-8"
    )
    for token in ("class AsyncSearchFacetResultEffects", "HandleAsyncSearchFacetResult("):
        assert token in header
    for forbidden in (
        "AppState", "Database", "CoverModule", "ImGui", "main.cpp",
        "WindowsMain", "platform/",
    ):
        assert forbidden not in header
        assert forbidden not in source
    apply_body = main
    assert adapter.count("HandleAsyncSearchFacetResult(") == 1
    assert "HandleAsyncSearchFacetResult(" not in apply_body
    assert apply_body.count("facetResultAdapter.apply();") == 1

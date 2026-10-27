from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
CMAKE = (ROOT / "cmake/harken_search_behavior_tests.cmake").read_text(encoding="utf-8")


def _toolbar_status_block() -> str:
    start = MAIN.index("const bool scanOwnsToolbarStatus")
    end = MAIN.index("// 3. TOOLBAR NAV", start)
    return MAIN[start:end]


def test_scan_owns_toolbar_status_during_active_or_foreground_completion() -> None:
    assert "TestScanQueryStatusContract" in CMAKE
    block = _toolbar_status_block()
    assert "state.isScanning.load" in block
    assert "state.scanCompletionPending.load" in block
    assert "if (!scanOwnsToolbarStatus)" in block


def test_transient_query_status_tags_remain_available_outside_scan() -> None:
    block = _toolbar_status_block()
    assert 'drawToolbarGlassTag("..."' in block
    assert "drawToolbarGlassTag(L(Search_Querying)" in block
    assert "state.filterQueryState == FilterQueryState::Ready" not in block
    assert "Search_NoResult" not in block

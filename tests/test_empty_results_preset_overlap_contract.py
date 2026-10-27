from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
CMAKE = (ROOT / "cmake/harken_search_behavior_tests.cmake").read_text(encoding="utf-8")


def _empty_results_block() -> str:
    start = MAIN.index("// UX #109: Empty state")
    end = MAIN.index("// Apply per-tab scroll restoration", start)
    return MAIN[start:end]


def _file_table_surface_block() -> str:
    start = MAIN.index("// File table with sortable columns")
    end = MAIN.index("// F-034 Relink", start)
    return MAIN[start:end]


def _search_status_block() -> str:
    start = MAIN.index("// 物理隔离：状态区固定 75px 占位")
    end = MAIN.index("// F-034: Column view preset dial", start)
    return MAIN[start:end]


def test_zero_results_do_not_render_a_result_layer_message() -> None:
    assert "TestEmptyResultsPresetOverlapContract" in CMAKE
    block = _empty_results_block()
    assert "TableNextRow(0, 64.0f)" not in block
    assert "RenderNoDatabaseSetupEmptyState(state)" not in block
    assert "Empty_NoResults" not in block


def test_zero_results_do_not_render_a_search_toolbar_message() -> None:
    block = _search_status_block()
    assert 'TextDisabled(u8"无结果")' not in block
    assert "totalFileCount == 0" not in block


def test_zero_results_do_not_render_a_toolbar_glass_tag() -> None:
    assert "drawToolbarGlassTag(L(Search_NoResult)" not in MAIN


def test_database_setup_guidance_stays_outside_the_result_layer() -> None:
    block = _empty_results_block()
    assert "Empty_NoFiles" not in block


def test_database_setup_surface_precedes_the_file_table() -> None:
    block = _file_table_surface_block()
    table_start = block.index('BeginTransientScrollbarTable("FileTableV6"')

    assert "state.startupNeedsDatabaseSetup && !state.database.isOpen()" in block
    assert "RenderNoDatabaseSetupEmptyState(state)" in block[:table_start]
    assert "RenderNoDatabaseSetupEmptyState(state)" not in block[table_start:]


def test_zero_result_guidance_is_present_on_a_dedicated_visible_surface() -> None:
    surface = _file_table_surface_block()
    assert "BeginTransientScrollbarChild(" in surface
    assert '"##EmptyResultsSurface"' in surface
    for message in (
        "Empty_NoFiles",
        "Empty_FilterAndSearch",
        "Empty_FilterOnly",
        "Empty_SearchOnly",
        "Empty_NoResults",
    ):
        assert message in surface


def test_zero_result_guidance_recognizes_all_visible_database_filters() -> None:
    surface = _file_table_surface_block()
    for active_filter_signal in (
        "state.categoryFilterBuffer[0] != '\\0'",
        "state.formatFilterIndex != 0",
        "state.channelFilterIndex != 0",
        "state.minDurationFilter > 0.0f",
        "state.maxDurationFilter > 0.0f",
        "state.minBpmFilter > 0.0f",
        "state.maxBpmFilter > 0.0f",
        "state.coverFilterIndex != 0",
        "state.activeSmartCollection != SmartCollection::None",
        "state.activeUserCollectionIdx >= 0",
        "state.genrePillState",
        "state.instrumentPillState",
        "!(state.showWAV && state.showMP3 && state.showFLAC && state.showOGG &&",
    ):
        assert active_filter_signal in surface


def test_zero_result_surface_reserves_space_before_the_player_footer() -> None:
    surface = _file_table_surface_block()
    assert "kEmptyResultsSurfaceHeight" in surface
    assert "fileTableOuterSize" in MAIN
    assert "-kEmptyResultsSurfaceHeight" in MAIN
    assert "ImGui::EndChild();" in surface
    assert MAIN.index("// UX #109: Empty state") < MAIN.index(
        "// INDEPENDENT PLAYER FOOTER CONTROL STRIP"
    )

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
CMAKE = (ROOT / "cmake/harken_search_behavior_tests.cmake").read_text(encoding="utf-8")


def _file_table_surface_block() -> str:
    start = MAIN.index("// File table with sortable columns")
    end = MAIN.index("// F-034 Relink", start)
    return MAIN[start:end]


def _assignment(source: str, needle: str) -> str:
    start = source.index(needle)
    end = source.index(";", start)
    return source[start:end]


def test_empty_list_inner_chrome_contract_is_wired() -> None:
    assert "TestEmptyListInnerChromeContract" in CMAKE


def test_truly_empty_list_does_not_reserve_inner_empty_surface() -> None:
    surface = _file_table_surface_block()
    cond = _assignment(surface, "const bool showEmptyResultsSurface")
    assert "totalFileCount == 0" in cond
    assert "searchResults.empty()" in cond


def test_truly_empty_list_does_not_draw_inner_info_child() -> None:
    surface = _file_table_surface_block()
    cond = _assignment(surface, "const bool showStandaloneEmptyResultsSurface")
    assert "totalFileCount == 0" in cond
    assert '"##EmptyResultsSurface"' in surface
    assert "Empty_NoFiles" in surface


def test_truly_empty_list_does_not_keep_table_outer_border() -> None:
    surface = _file_table_surface_block()
    flags_start = surface.index("ImGuiTableFlags tableFlags")
    table_start = surface.index('BeginTransientScrollbarTable("FileTableV6"')
    flags = surface[flags_start:table_start]
    assert "ImGuiTableFlags_BordersOuter" in flags
    assert "totalFileCount == 0" in flags


def test_non_empty_list_keeps_existing_empty_filter_guidance() -> None:
    surface = _file_table_surface_block()
    for message in (
        "Empty_FilterAndSearch",
        "Empty_FilterOnly",
        "Empty_SearchOnly",
        "Empty_NoResults",
    ):
        assert message in surface


if __name__ == "__main__":
    test_empty_list_inner_chrome_contract_is_wired()
    test_truly_empty_list_does_not_reserve_inner_empty_surface()
    test_truly_empty_list_does_not_draw_inner_info_child()
    test_truly_empty_list_does_not_keep_table_outer_border()
    test_non_empty_list_keeps_existing_empty_filter_guidance()
    print("PASS")

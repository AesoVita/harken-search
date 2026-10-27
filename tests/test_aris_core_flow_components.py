from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "src" / "main.cpp"
WIDGETS_H = ROOT / "src" / "HarkenSearchWidgets.h"
WIDGETS_CPP = ROOT / "src" / "HarkenSearchWidgets.cpp"


def _function_body(source: str, marker: str, end_marker: str) -> str:
    start = source.index(marker)
    end = source.index(end_marker, start)
    return source[start:end]


def test_search_flow_uses_the_shared_aris_text_input_contract():
    main = MAIN.read_text(encoding="utf-8")
    widgets_h = WIDGETS_H.read_text(encoding="utf-8")
    widgets_cpp = WIDGETS_CPP.read_text(encoding="utf-8")

    assert '#include "HarkenSearchWidgets.h"' in main
    search_input = _function_body(
        main,
        "static bool DrawUnifiedSearchInput(",
        "// Renders [optional search field]",
    )
    assert "aura::ui::TextInput" in search_input
    assert 'DrawUnifiedSearchInput("##SearchBar"' in main
    assert '"##FolderSearch"' in main

    assert "ImGuiInputTextFlags inputFlags" in widgets_h
    assert "opts.inputFlags" in widgets_cpp


def test_search_component_preserves_enter_to_submit_behavior():
    main = MAIN.read_text(encoding="utf-8")
    widgets_cpp = WIDGETS_CPP.read_text(encoding="utf-8")

    search_input = _function_body(
        main,
        "static bool DrawUnifiedSearchInput(",
        "// Renders [optional search field]",
    )
    assert "flags" in search_input
    assert "inputFlags" in search_input
    assert "opts.inputFlags" in widgets_cpp

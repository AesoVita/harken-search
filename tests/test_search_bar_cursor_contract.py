from pathlib import Path


MAIN = Path(__file__).resolve().parents[1] / "src" / "main.cpp"


def _search_bar_region() -> str:
    source = MAIN.read_text(encoding="utf-8")
    start = source.index('DrawUnifiedSearchInput("##SearchBar"')
    end = source.index('ImGui::PopStyleColor(colorPushes);', start)
    return source[start:end]


def test_search_bar_uses_native_caret_without_redundant_custom_overlay():
    region = _search_bar_region()
    assert "Draw the custom breathing caret" not in region
    assert "AddRectFilled" not in region

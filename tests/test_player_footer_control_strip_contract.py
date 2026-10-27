from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
CMAKE = (ROOT / "cmake/harken_search_behavior_tests.cmake").read_text(encoding="utf-8")


def _strip_block() -> str:
    start = MAIN.index("// INDEPENDENT PLAYER FOOTER CONTROL STRIP")
    end = MAIN.index(
        "ImGui::SetCursorScreenPos(ImVec2(pitchWheelCenteredX, pitchColumnY));",
        start,
    )
    return MAIN[start:end]


def test_player_footer_strip_uses_theme_surface_across_full_width() -> None:
    assert "TestPlayerFooterControlStripContract" in CMAKE
    block = _strip_block()
    assert "ResolveWaveformFooterSurface(playerFooterStripTheme)" in block
    assert "playerFooterStripMin" in block
    assert "playerFooterStripMax" in block
    assert "pitchColumnOriginX" in block
    assert "GetWindowContentRegionMax" in block
    assert "pitchColumnY + availHeight" in block
    assert "pitchColumnY + panelAvailH" in block
    assert "AddRectFilledMultiColor" in block
    assert "AddLine" in block


def test_player_footer_strip_does_not_reserve_layout_space() -> None:
    block = _strip_block()
    assert "ImGui::Dummy" not in block
    assert "panelAvailH - reservedH" not in block
    assert "ImGui::InvisibleButton" not in block

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "src/main.cpp"

def test_ready_update_popup_uses_visible_panel_instead_of_blank_modal() -> None:
    source = MAIN.read_text(encoding="utf-8")
    notice = (ROOT / "src/update/HarkenSearchUpdateNoticeUi.cpp").read_text(encoding="utf-8")
    popup = notice[notice.index("void RenderHarkenSearchUpdateNoticeUi("):]
    assert "aris::update::RenderHarkenSearchUpdateNoticeUi(" in source
    assert "kUpdateNoticePopupId" not in source
    assert "ImGui::PushStyleColor(ImGuiCol_WindowBg, theme.dialogBG);" in popup
    assert "ImGui::PushStyleColor(ImGuiCol_Border, theme.chromeBorder);" in popup
    assert "ImGui::Begin(presentation.windowId, &open, flags)" in popup
    assert "RenderHeader(open, notes, theme);" in popup
    assert "RenderStatusRow(state, updateState, theme);" in popup
    assert "RenderVersionCard(state, updateState, theme);" in popup
    assert "ImGui::PopStyleColor(2);" in popup

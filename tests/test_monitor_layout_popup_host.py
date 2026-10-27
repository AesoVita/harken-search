from pathlib import Path


MAIN = (Path(__file__).resolve().parents[1] / "src" / "main.cpp").read_text(encoding="utf-8")


def test_monitor_layout_combo_is_outside_scrolling_settings_clip() -> None:
    start = MAIN.index("BeginTransientScrollbarPopupModal(L(Dlg_Settings)")
    end = MAIN.index("settingsDialogOpenNow", start)
    popup = MAIN[start:end]
    combo_at = popup.index('ImGui::Combo("##MonitorLayout"')
    clip_at = popup.index("ImGui::PushClipRect(", popup.index("const ImVec2 settingsBodyPos("))
    clip_end = popup.index("ImGui::PopClipRect();", clip_at)
    assert combo_at < clip_at or combo_at > clip_end
    assert combo_at < popup.index("ImGui::BeginGroup();", clip_at)

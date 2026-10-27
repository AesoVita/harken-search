import pathlib
import re
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


def _read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8")


def _settings_dialog(main: str) -> str:
    start = main.index("if (BeginTransientScrollbarPopupModal(L(Dlg_Settings)")
    end = main.index("s_settingsDialogWasOpen = settingsDialogOpenNow")
    return main[start:end]


def _settings_search_entries(main: str) -> str:
    start = main.index("struct SettingsSearchEntry")
    end = main.index("auto tabMatchesSearch")
    return main[start:end]


def _settings_toggle_row(widgets: str) -> str:
    start = widgets.index("bool SettingsToggleRow(")
    end = widgets.index("bool SettingsSliderRow(", start)
    return widgets[start:end]


def _settings_button_row(widgets: str) -> str:
    start = widgets.index("bool SettingsButtonRow(")
    end = widgets.index("void TooltipRich(", start)
    return widgets[start:end]


class SettingsSearchHitHighlightSourceContractTests(unittest.TestCase):
    def test_gui_indexes_guided_mode_control_not_only_section(self):
        entries = _settings_search_entries(_read("src/main.cpp"))
        self.assertIn("L(Set_GuidedMode)", entries)
        guided = entries.split("L(Set_GuidedMode)", 1)[1].split("addEntry", 1)[0]
        self.assertIn("guided", guided.lower())
        self.assertNotIn(
            'addEntry(5, L(Set_UIElements), u8"界面 引导 tooltip ui guided")',
            entries,
        )

    def test_guided_mode_label_draw_uses_hit_spans_when_search_active(self):
        main = _read("src/main.cpp")
        widgets_h = _read("src/HarkenSearchWidgets.h")
        widgets = _read("src/HarkenSearchWidgets.cpp")
        dialog = _settings_dialog(main)

        self.assertIn("BeginSettingsSearchHighlight", dialog)
        self.assertIn("activeSettingsSearchEntry->keywords", dialog)
        self.assertIn("g_settingsSearchKeywords", widgets)
        self.assertIn("NotifySettingsSearchLabeledItem", _settings_toggle_row(widgets))
        self.assertIn("NotifySettingsSearchLabeledItem", _settings_button_row(widgets))
        self.assertIn("DrawSettingsRowLabel", widgets)
        self.assertIn("NotifySettingsSearchLabeledItem", widgets)
        self.assertIn("BuildSettingsLabelHighlightSpans", widgets)
        spans_h = _read("src/SettingsSearchHighlightSpans.h")
        self.assertIn("FindPinyinSpanUtf8", spans_h)
        self.assertIn("FindAsciiCaseInsensitiveSpan", spans_h)
        self.assertIn("EnsureActiveSettingsLabelHighlight", spans_h)
        self.assertIn("const std::string& keywords", spans_h)
        self.assertIn("EnsureActiveSettingsLabelHighlight", widgets)
        self.assertIn("AddRectFilled", widgets)
        self.assertIn("0.38f", widgets)
        self.assertIn("resultsSelection", widgets)
        self.assertIn("BeginSettingsSearchHighlight", widgets_h)
        self.assertIn("NotifySettingsSearchLabeledItem", widgets_h)

        toggle_at = dialog.index("if (aura::ui::SettingsToggleRow(\n                        L(Set_GuidedMode),")
        self.assertIn("&state.settings.showGuidedMode", dialog[toggle_at:toggle_at + 280])

        checkbox_fn = widgets[widgets.index("bool Checkbox(") : widgets.index("bool Radio(")]
        self.assertIn("NotifySettingsSearchLabeledItem", checkbox_fn)
        radio_fn = widgets[widgets.index("bool Radio(") : widgets.index("bool Toggle(")]
        self.assertIn("NotifySettingsSearchLabeledItem", radio_fn)

        section = widgets[widgets.index("bool SectionHeader(") : widgets.index("ImVec4 LerpColor(")]
        self.assertIn("NotifySettingsSearchLabeledItem(title", section)
        self.assertNotIn("NotifySettingsSearchLabeledItem(buf", section)

        overlay = widgets[
            widgets.index("void OverlayLabelHighlight(") : widgets.index("void BeginSettingsSearchHighlight(")
        ]
        self.assertIn("SettingsLabelIsCurrentHit", overlay)
        self.assertNotIn("if (label != g_settingsSearchActiveLabel) return;", overlay)

        notify = widgets[widgets.index("void NotifySettingsSearchLabeledItem") :]
        notify = notify[: notify.index("\nvoid ", 1) if "\nvoid " in notify[1:] else 2500]
        self.assertIn("SettingsLabelIsCurrentHit", notify)

        self.assertIn("DrawSettingsSearchableLabel", widgets_h)
        self.assertIn("DrawSettingsSearchableLabel", widgets)

    def test_settings_page_checkbox_and_radio_labels_join_hit_highlight(self):
        dialog = _settings_dialog(_read("src/main.cpp"))
        self.assertIn("aura::ui::Checkbox(L(Set_AutoRenameTab)", dialog)
        self.assertIn("aura::ui::Radio(L(Set_RealtimeSearch)", dialog)
        self.assertIn("aura::ui::Radio(L(Set_EnterSearch)", dialog)
        self.assertIn("aura::ui::Checkbox(L(Set_RememberLastFolder)", dialog)
        self.assertNotIn("ImGui::Checkbox(L(Set_AutoRenameTab)", dialog)
        self.assertNotIn("ImGui::RadioButton(L(Set_RealtimeSearch)", dialog)
        self.assertIn("DrawSettingsSearchableLabel(L(Set_UILanguage)", dialog)
        self.assertIn("aura::ui::Radio(L(Main_LangChinese)", dialog)
        self.assertIn("aura::ui::Radio(L(Main_LangEnglish)", dialog)
        self.assertIn("aura::ui::Radio(L(Main_LangJapanese)", dialog)

    def test_settings_index_includes_language_and_output_suffix_controls(self):
        entries = _settings_search_entries(_read("src/main.cpp"))
        for key in (
            "L(Main_LangChinese)",
            "L(Main_LangEnglish)",
            "L(Main_LangJapanese)",
            "L(Set_UILanguage)",
            "L(Main_ArisOutputSuffixCheck)",
        ):
            self.assertIn(key, entries)

    def test_current_hit_updates_custom_settings_body_scroll(self):
        main = _read("src/main.cpp")
        dialog = _settings_dialog(main)
        self.assertIn("s_settingsBodyScroll", dialog)
        self.assertIn("TakeSettingsSearchHitLayout", dialog)
        self.assertRegex(
            dialog,
            r"s_settingsBodyScroll\s*\+=",
        )
        self.assertNotIn("SetScrollHereY", dialog)
        widgets_h = _read("src/HarkenSearchWidgets.h")
        self.assertIn("TakeSettingsSearchHitLayout", widgets_h)

    def test_arrow_buttons_change_settings_search_cursor(self):
        dialog = _settings_dialog(_read("src/main.cpp"))
        self.assertIn('ArrowButton("##SettingsSearchPrev"', dialog)
        self.assertIn('ArrowButton("##SettingsSearchNext"', dialog)
        self.assertIn("--settingsSearchCursor", dialog)
        self.assertIn("++settingsSearchCursor", dialog)

    def test_collapsing_headers_notify_layout_for_current_hit(self):
        dialog = _settings_dialog(_read("src/main.cpp"))
        for title in (
            "L(Set_OutputHardware)",
            "L(Set_PlaybackBehavior)",
            "L(Set_PerfMode)",
            "L(Set_RenderStyle)",
        ):
            self.assertIn(f"ImGui::CollapsingHeader({title}", dialog)
            self.assertIn(
                f"NotifySettingsSearchLabeledItem({title}",
                dialog,
            )

    def test_radio_keeps_caller_text_color(self):
        widgets = _read("src/HarkenSearchWidgets.cpp")
        radio_fn = widgets[widgets.index("bool Radio(") : widgets.index("bool Toggle(")]
        self.assertIn("GetStyleColorVec4(ImGuiCol_Text)", radio_fn)
        self.assertNotIn("th.interfaceText", radio_fn)

    def test_empty_query_does_not_force_label_highlight(self):
        widgets = _read("src/HarkenSearchWidgets.cpp")
        notify = widgets[widgets.index("void NotifySettingsSearchLabeledItem") :]
        notify = notify[: notify.index("\nvoid ", 1) if "\nvoid " in notify[1:] else 2500]
        self.assertIn("g_settingsSearchQuery", notify)
        self.assertTrue(
            "empty()" in notify or "empty" in notify,
            "highlight path must bail out when the settings query is empty",
        )

    def test_overlay_pulses_hit_glyphs_then_settles_steady(self):
        widgets = _read("src/HarkenSearchWidgets.cpp")
        overlay = widgets[widgets.index("void OverlayLabelHighlight(") : widgets.index("void BeginSettingsSearchHighlight(")]
        self.assertIn("ImGui::GetTime()", overlay)
        executable_overlay = re.sub(r"/\*.*?\*/", "", overlay, flags=re.S)
        executable_overlay = re.sub(r"//.*?$", "", executable_overlay, flags=re.M)
        self.assertIn("const float alpha = SettingsSearchHighlightAlpha(elapsed);", executable_overlay)
        self.assertNotIn("0.15f", overlay)
        self.assertNotIn("0.55f", overlay)
        self.assertNotIn("0.38f", overlay)
        self.assertIn("spans.empty()", overlay)
        self.assertIn("AddRectFilled", overlay)
        # Pulse only the matching glyphs, not the whole control.
        self.assertIn("hiSize", overlay)
        self.assertNotIn("itemMax.x, itemMax.y", overlay)

        begin = widgets[widgets.index("void BeginSettingsSearchHighlight(") : widgets.index("void EndSettingsSearchHighlight(")]
        self.assertIn("ImGui::GetTime()", begin)
        self.assertIn("g_settingsSearchLastOverlayId = 0", begin)
        self.assertIn("g_settingsSearchQuery", begin)
        self.assertIn("g_settingsSearchActiveLabel", begin)
        self.assertIn("g_settingsSearchKeywords", begin)


if __name__ == "__main__":
    unittest.main()

"""RQ-20260920-02 follow-up: results/waveform boundary splitter contract."""

from __future__ import annotations

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "src" / "main.cpp"


def _read() -> str:
    return MAIN.read_text(encoding="utf-8")


class WaveformSplitterContract(unittest.TestCase):
    def test_waveform_boundary_has_dedicated_splitter_id(self) -> None:
        self.assertIn('HorizontalSplitter("##WaveformSplitter"', _read())

    def test_splitter_binds_only_waveform_docked_height(self) -> None:
        main = _read()
        idx = main.find('HorizontalSplitter("##WaveformSplitter"')
        self.assertGreater(idx, 0)
        block = main[idx : idx + 800]
        self.assertIn("&state.panelWaveform.dockedHeight", block)
        self.assertIn(
            "state.settings.waveformPanelHeight = state.panelWaveform.dockedHeight;",
            block,
        )
        self.assertIn("MarkSettingsDirty(state);", block)

    def test_height_budget_consumes_waveform_docked_height(self) -> None:
        main = _read()
        self.assertIn("desiredWaveformH", main)
        budget = main.split("desiredWaveformH", 1)[1][:300]
        self.assertIn("state.panelWaveform.dockedHeight", budget)

    def test_drag_preview_bottom_is_shared_and_not_hardcoded_blue(self) -> None:
        main = _read()
        self.assertIn("ResolveWaveformSplitterDragBg", main)
        self.assertIn("waveformSplitterDragBg", main)
        self.assertIn("gripFillBase", main)
        self.assertNotIn(
            "IM_COL32(90, 170, 240, aura::ui::dock::DockSplitGripFillAlpha(hovered, dragging))",
            main,
        )
        self.assertIn(
            "splitterMix(transportSeamBg, splitterTheme.chromeBorder, 0.06f)", main
        )

    def test_idle_dark_base_active_drag_preview(self) -> None:
        main = _read()
        self.assertIn('HorizontalSplitter("##TopDockSplitter"', main)
        self.assertIn('HorizontalSplitter("##BottomDockSplitter"', main)
        self.assertIn("const ImVec4 idleSeamBg = transportSeamBg;", main)
        self.assertIn("? waveformSplitterDragBg", main)
        self.assertNotIn("(waveformSplitter || dockBandSplitter)", main)


if __name__ == "__main__":
    unittest.main()

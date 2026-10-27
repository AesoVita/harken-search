import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "src" / "main.cpp"


class MagicEQSpectrumContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        source = MAIN.read_text(encoding="utf-8")
        render_ui = source[source.index("void RenderUI(AppState& state) {"):]
        spectrum_start = render_ui.index(
            "if (state.showSpectrogram && state.audioEngine.isLoaded())"
        )
        split_start = render_ui.index("if (state.spectrogramSplit)", spectrum_start)
        merged_start = render_ui.index(
            "// MERGED MODE: Spectrogram underneath waveform", split_start
        )
        waveform_only = render_ui.index(
            "// No spectrogram - just draw waveform", merged_start
        )
        cls.split_scope = render_ui[split_start:merged_start]
        cls.merged_scope = render_ui[merged_start:waveform_only]
        overlay_start = source.index("static void DrawMagicEQSelectionOverlay(")
        body_start = source.index("{", overlay_start) + 1
        overlay_end = source.index("static void FormatSpectrogramFrequencyLabel(", body_start)
        cls.overlay = source[body_start:overlay_end]
        cls.frequency_map = source[
            source.index("static float SpectrogramFrequencyToScreenY("):
            overlay_start
        ]

    def test_split_spectrum_path_draws_live_selection_after_its_texture(self) -> None:
        image = self.split_scope.index("dl->AddImage(")
        overlay = self.split_scope.index("DrawMagicEQSelectionOverlay(")
        self.assertLess(image, overlay)
        self.assertIn("state.audioEngine.getMagicEQ()", self.split_scope[overlay:])
        self.assertIn("CanDisplaySpectrogramTextureForTimeline(", self.split_scope)

    def test_merged_spectrum_path_draws_selection_above_spectrum_and_waveform(self) -> None:
        image = self.merged_scope.index("dl->AddImage(")
        overlay = self.merged_scope.index("DrawMagicEQSelectionOverlay(")
        waveform = self.merged_scope.rfind("DrawMainWaveform(")
        self.assertLess(image, overlay)
        self.assertGreaterEqual(waveform, 0)
        self.assertGreater(overlay, waveform)
        self.assertIn("state.audioEngine.getMagicEQ()", self.merged_scope[overlay:])
        self.assertIn("CanDisplaySpectrogramTextureForTimeline(", self.merged_scope)

    def test_current_frequency_edges_map_to_clipped_visible_rows(self) -> None:
        self.assertIn("CenterSenseDsp::magicEQEdges(settings.centerFreq", self.overlay)
        self.assertIn("settings.halfWidthPercent", self.overlay)
        self.assertIn("SpectrogramFrequencyToScreenY(rectMin, rectMax, highHz, sampleRate)", self.overlay)
        self.assertIn("SpectrogramFrequencyToScreenY(rectMin, rectMax, lowHz, sampleRate)", self.overlay)
        self.assertIn("std::clamp(yHigh, rectMin.y, rectMax.y)", self.overlay)
        self.assertIn("std::clamp(yLow, rectMin.y, rectMax.y)", self.overlay)
        self.assertIn("drawList->AddRectFilled", self.overlay)
        self.assertEqual(self.overlay.count("drawList->AddLine"), 3)
        self.assertIn("SpectrogramFrequencyHzToReadableNorm(frequencyHz, sampleRate)", self.frequency_map)

    def test_overlay_uses_live_selection_and_mode_instead_of_cached_scope_state(self) -> None:
        self.assertNotIn("static MagicEQSettings", self.overlay)
        self.assertNotIn("static float lowHz", self.overlay)
        self.assertIn("settings.mode == MagicEQMode::Notch", self.overlay)
        self.assertIn("if (!settings.enabled) return;", self.overlay)
        for scope in (self.split_scope, self.merged_scope):
            self.assertIn("state.audioEngine.getMagicEQ()", scope)
            self.assertIn("(float)fileInfo.sampleRate", scope)

    def test_pass_dims_outside_notch_dims_inside_and_bypass_has_no_veil(self) -> None:
        color = re.search(r"bandColor = IM_COL32\((\d+), (\d+), (\d+), (\d+)\)", self.overlay)
        self.assertIsNotNone(color, "the veil must have an explicit neutral color")
        red, green, blue, alpha = map(int, color.groups())
        # Judge the visual effect relative to a representative warm spectrum
        # pixel, rather than freezing the implementation's opacity.
        source = (210, 123, 39)
        masked = tuple(round(original * (255 - alpha) / 255 + veil * alpha / 255)
                       for original, veil in zip(source, (red, green, blue)))
        luminance = lambda rgb: sum(channel * weight for channel, weight in
                                    zip(rgb, (0.2126, 0.7152, 0.0722)))
        self.assertLess(luminance(masked), luminance(source) * 0.8,
                        "the selected processing area must be visibly darker")

        bypass = self.overlay.index("if (!settings.enabled) return;")
        mode = self.overlay.index("if (settings.mode == MagicEQMode::Notch)")
        self.assertLess(bypass, mode)
        notch, pass_mode = self.overlay[mode:].split("} else {", 1)
        self.assertIn("AddRectFilled(ImVec2(rectMin.x, yHigh), ImVec2(rectMax.x, yLow), bandColor)", notch)
        self.assertEqual(notch.count("AddRectFilled"), 1)
        self.assertIn("AddRectFilled(ImVec2(rectMin.x, rectMin.y), ImVec2(rectMax.x, yHigh), bandColor)", pass_mode)
        self.assertIn("AddRectFilled(ImVec2(rectMin.x, yLow), ImVec2(rectMax.x, rectMax.y), bandColor)", pass_mode)
        self.assertEqual(pass_mode.split("}", 1)[0].count("AddRectFilled"), 2)

if __name__ == "__main__":
    unittest.main()

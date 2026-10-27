import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
APP = ROOT / "src" / "webui" / "src" / "App.tsx"
STYLES = ROOT / "src" / "webui" / "src" / "styles" / "flash-0225.css"
MINI_WAVEFORM = ROOT / "src" / "webui" / "src" / "components" / "waveform" / "MiniWaveform.tsx"


class WebUiResultRenderBudgetContract(unittest.TestCase):
    def test_result_rows_skip_offscreen_render_work_without_virtualizing_selection(self) -> None:
        app = APP.read_text(encoding="utf-8")
        styles = STYLES.read_text(encoding="utf-8")

        self.assertIn(
            "{displayRows.map((row, idx) => {",
            app,
            "keep the complete result DOM so selection indexes, keyboard traversal, and scrolling stay unchanged",
        )
        self.assertIn("content-visibility: auto;", styles)
        self.assertIn("contain-intrinsic-block-size: auto var(--row-h);", styles)

    def test_offscreen_rows_do_not_request_or_animate_mini_waveforms(self) -> None:
        app = APP.read_text(encoding="utf-8")
        mini_waveform = MINI_WAVEFORM.read_text(encoding="utf-8")

        self.assertIn("const [resultVisibleRange, setResultVisibleRange]", app)
        self.assertIn(
            "renderActive={idx >= resultVisibleRange.start && idx < resultVisibleRange.end}",
            app,
        )
        self.assertIn("renderActive: boolean;", mini_waveform)
        self.assertGreaterEqual(
            mini_waveform.count("if (!renderActive) return;"),
            2,
            "both waveform RPC and canvas/RAF work must be gated by the visible overscan range",
        )
        self.assertIn("setCacheRevision((value) => value + 1);", mini_waveform)
        self.assertIn(
            "[fileId, width, height, isPlaying, playPos, renderActive, cacheRevision]",
            mini_waveform,
            "a completed waveform RPC must redraw an otherwise memoized static row",
        )


if __name__ == "__main__":
    unittest.main()

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / "src" / "webui" / "src" / "App.tsx"


def transport_polling_block(app: str) -> str:
    start = app.index("// Poll transport state")
    end = app.index("// Global transport hotkey", start)
    return app[start:end]


class WebUiTransportPollingBudgetContract(unittest.TestCase):
    def test_transport_polling_is_single_flight(self) -> None:
        app = APP.read_text(encoding="utf-8")
        polling = transport_polling_block(app)

        self.assertIn("let inFlight = false;", polling)
        self.assertIn("if (!alive || inFlight) return;", polling)
        self.assertIn("inFlight = true;", polling)
        self.assertIn("finally {\n        inFlight = false;\n      }", polling)

    def test_unchanged_transport_snapshot_reuses_previous_state(self) -> None:
        app = APP.read_text(encoding="utf-8")

        self.assertIn("function sameTransportState", app)
        for field in (
            "playing",
            "loaded",
            "position",
            "duration",
            "pitch",
            "volume",
            "inPoint",
            "outPoint",
            "fileId",
            "reversed",
        ):
            self.assertIn(f"left.{field} === right.{field}", app)
        self.assertIn(
            "setTransport((previous) => sameTransportState(previous, s) ? previous : s);",
            app,
        )


if __name__ == "__main__":
    unittest.main()

"""Similar Sounds default hidden contract."""

from __future__ import annotations

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WIN = ROOT / "src" / "WindowsMain.cpp"
MAIN = ROOT / "src" / "main.cpp"


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


class SimilarSoundsDefaultHiddenContract(unittest.TestCase):
    def test_default_state_hides_real_similar_sounds(self) -> None:
        win = _read(WIN)
        self.assertIn("state.panelSimilarSounds.visible = false;", win)

    def test_real_panel_has_no_similar_test_registration(self) -> None:
        main = _read(MAIN)
        registration = main.split('static const char* const kDockTestModuleIds', 1)[1].split('};', 1)[0]
        self.assertNotIn("mod-docktest-similar", registration)
        self.assertNotIn("mod-docktest-similar-sounds", main)

    def test_explicit_open_path_still_sets_visible(self) -> None:
        similar = _read(ROOT / "src" / "SimilarSoundsWindow.cpp")
        self.assertIn("state.panelSimilarSounds.visible = true;", similar)


if __name__ == "__main__":
    unittest.main()

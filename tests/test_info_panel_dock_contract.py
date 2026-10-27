"""Docked Info keeps its title actions and content inside the shell."""

from __future__ import annotations

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "src" / "main.cpp"


def _read() -> str:
    return MAIN.read_text(encoding="utf-8")


class InfoPanelDockContract(unittest.TestCase):
    def test_title_actions_are_shared_by_legacy_and_tree_paths(self) -> None:
        main = _read()
        self.assertIn("drawFileInfoTitleActions", main)
        right_column = main[
            main.index("// Right Column - File Info with title bar"):
            main.index("if (g_dockTestBandHintsVisible)")
        ]
        tree_info = main[
            main.index('if (leaf.id == "info"'):
            main.index('if (leaf.id == "similar-sounds"')
        ]
        self.assertIn("drawFileInfoTitleActions", right_column)
        self.assertIn("drawFileInfoTitleActions", tree_info)

    def test_title_drag_reserves_both_action_buttons(self) -> None:
        main = _read()
        self.assertIn("kInfoTitleActionsWidth", main)
        self.assertIn("shell.shellWidth - kInfoTitleActionsWidth", main)
        self.assertIn("rightShell.shellWidth - kInfoTitleActionsWidth", main)

    def test_metadata_artwork_size_is_bounded_by_panel_width(self) -> None:
        main = _read()
        self.assertIn("kInfoMinArtworkSize", main)
        self.assertIn("panelWidth - 20.0f", main)
        self.assertIn("std::min)(260.0f", main)


if __name__ == "__main__":
    unittest.main()

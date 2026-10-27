"""Info dock content geometry: no overlap with a lower docked panel."""

from __future__ import annotations

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "src" / "main.cpp"


def _read() -> str:
    return MAIN.read_text(encoding="utf-8")


class InfoSimilarDockGeometryContract(unittest.TestCase):
    def test_info_content_uses_info_leaf_height_not_full_column(self) -> None:
        main = _read()
        idx = main.find('"FileInfoContent"')
        self.assertGreater(idx, 0)
        block = main[idx : idx + 800]
        self.assertIn("rightShellHeight - rightShell.titleHeight", block)
        self.assertNotIn("availHeight - rightShell.titleHeight", block)

    def test_similar_docked_content_is_constrained_to_the_inner_leaf(self) -> None:
        main = _read()
        idx = main.find('"##DockedSimilarSounds"')
        self.assertGreater(idx, 0)
        block = main[idx : idx + 5000]
        self.assertIn('"##DockedSimilarSoundsContent"', block)
        self.assertIn("similarShell.insetL + similarShell.innerPad", block)
        self.assertIn("similarShell.insetT + similarShell.titleHeight + 4.0f", block)
        self.assertIn(
            "leaf.rect.width - similarShell.insetL - similarShell.insetR -",
            block)
        self.assertIn(
            "leaf.rect.height - similarShell.insetT - similarShell.insetB -",
            block)


if __name__ == "__main__":
    unittest.main()

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "src/main.cpp"
HEADER = ROOT / "src/CoverWallFilterActivityPolicy.h"
SOURCE = ROOT / "src/CoverWallFilterActivityPolicy.cpp"


def test_cover_wall_filter_activity_has_independent_owner():
    assert HEADER.exists()
    assert SOURCE.exists()
    source = SOURCE.read_text(encoding="utf-8")
    for contract in (
        "HasActiveDbFilterForCoverWall",
        "BuildCurrentDbFilter",
        "coverFilter",
        "includeOnlyKnownExts",
    ):
        assert contract in source
    assert '"main.cpp"' not in source
    assert '"WindowsMain.h"' not in source
    assert '"platform/' not in source


def test_main_consumes_cover_wall_filter_activity_policy():
    main = MAIN.read_text(encoding="utf-8")
    assert '#include "CoverWallFilterActivityPolicy.h"' in main
    assert "static bool HasActiveDbFilterForCoverWall(" not in main
    assert "using aura::cover_wall::HasActiveDbFilterForCoverWall;" in main

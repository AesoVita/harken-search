from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "src/main.cpp"
HEADER = ROOT / "src/CoverWallFacetRefreshPolicy.h"
SOURCE = ROOT / "src/CoverWallFacetRefreshPolicy.cpp"


def test_cover_wall_facet_refresh_has_independent_owner():
    assert HEADER.exists()
    assert SOURCE.exists()
    source = SOURCE.read_text(encoding="utf-8")
    for contract in (
        "CoverWallItemsShareVisualOrder",
        "RefreshCoverWallFacetMetadataInPlace",
        "BuildCoverWallVisualDedupeToken",
    ):
        assert contract in source
    assert '"main.cpp"' not in source
    assert '"WindowsMain.h"' not in source
    assert '"platform/' not in source


def test_main_consumes_cover_wall_facet_refresh_policy():
    main = MAIN.read_text(encoding="utf-8")
    assert '#include "CoverWallFacetRefreshPolicy.h"' in main
    for old_owner in (
        "static bool CoverWallItemsShareVisualOrder(",
        "static void RefreshCoverWallFacetMetadataInPlace(",
    ):
        assert old_owner not in main
    for symbol in (
        "CoverWallItemsShareVisualOrder",
        "RefreshCoverWallFacetMetadataInPlace",
    ):
        assert f"using aura::cover_wall::{symbol};" in main

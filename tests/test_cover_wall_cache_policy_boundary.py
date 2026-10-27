from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "src/main.cpp"
HEADER = ROOT / "src/CoverWallCachePolicy.h"
SOURCE = ROOT / "src/CoverWallCachePolicy.cpp"


def test_cover_wall_cache_policy_has_independent_owner():
    assert HEADER.exists()
    assert SOURCE.exists()
    source = SOURCE.read_text(encoding="utf-8")
    for contract in (
        "CanReuseGlobalCoverWallCache",
        "StoreGlobalCoverWallCache",
        "LastCoverWallPageCursor",
        "ReconstructGlobalCoverWallPageState",
    ):
        assert contract in source
    assert '"main.cpp"' not in source
    assert '"WindowsMain.h"' not in source
    assert '"platform/' not in source


def test_main_consumes_cover_wall_cache_policy():
    main = MAIN.read_text(encoding="utf-8")
    assert '#include "CoverWallCachePolicy.h"' in main
    for old_owner in (
        "static bool CanReuseGlobalCoverWallCache(",
        "static void StoreGlobalCoverWallCache(",
        "static std::string LastCoverWallPageCursor(",
        "static void ReconstructGlobalCoverWallPageState(",
    ):
        assert old_owner not in main
    for symbol in (
        "CanReuseGlobalCoverWallCache",
        "StoreGlobalCoverWallCache",
        "LastCoverWallPageCursor",
        "ReconstructGlobalCoverWallPageState",
    ):
        assert f"using aura::cover_wall::{symbol};" in main


def test_return_from_facets_restarts_incomplete_global_pagination():
    main = MAIN.read_text(encoding="utf-8")
    start = main.index("// Global mode reads deduplicated covers from the database, then discovers more.")
    end = main.index("bool needStartLoad = false;", start)
    global_entry = main[start:end]
    assert "if (wasUsingFacetCoverWall && !canReuseGlobalCache) {" in global_entry
    assert "cw.prepareGlobalReloadAfterFacet(mainDbPath);" in global_entry
    assert global_entry.index("cw.prepareGlobalReloadAfterFacet(mainDbPath);") < global_entry.index("if (canReuseGlobalCache)")

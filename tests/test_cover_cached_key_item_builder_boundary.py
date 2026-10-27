from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "src/main.cpp"
HEADER = ROOT / "src/CoverCachedKeyItemBuilder.h"
SOURCE = ROOT / "src/CoverCachedKeyItemBuilder.cpp"


def test_cached_key_cover_items_have_independent_owner():
    assert HEADER.exists()
    assert SOURCE.exists()
    source = SOURCE.read_text(encoding="utf-8")
    for contract in (
        "IsRealCoverWallItemCandidate",
        "SanitizeCoverWallItemsForCache",
        "BuildCoverWallItemsFromCachedKeys",
        "BuildCoverWallItemsFromSearchResultsFallback",
        "TryGetFolderCoverKeyFast",
        "CanonicalFolderPath",
        "kMaxRecordsForCoverBuildOnUIThread",
    ):
        assert contract in source
    assert '"main.cpp"' not in source
    assert '"WindowsMain.h"' not in source
    assert '"platform/' not in source


def test_main_consumes_cached_key_cover_item_builder():
    main = MAIN.read_text(encoding="utf-8")
    assert '#include "CoverCachedKeyItemBuilder.h"' in main
    for old_owner in (
        "static bool IsRealCoverWallItemCandidate(",
        "static void SanitizeCoverWallItemsForCache(",
        "static void BuildCoverWallItemsFromCachedKeys(",
        "static void BuildCoverWallItemsFromSearchResultsFallback(",
    ):
        assert old_owner not in main
    for symbol in (
        "IsRealCoverWallItemCandidate",
        "SanitizeCoverWallItemsForCache",
        "BuildCoverWallItemsFromCachedKeys",
        "BuildCoverWallItemsFromSearchResultsFallback",
    ):
        assert f"using aura::cover_wall::{symbol};" in main

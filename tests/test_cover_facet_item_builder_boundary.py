from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "src/main.cpp"
HEADER = ROOT / "src/CoverFacetItemBuilder.h"
SOURCE = ROOT / "src/CoverFacetItemBuilder.cpp"


def test_cover_facet_item_builder_has_independent_owner():
    assert HEADER.exists()
    assert SOURCE.exists()
    source = SOURCE.read_text(encoding="utf-8")
    for contract in (
        "BuildCoverWallItemsFromCoverFacets",
        "CanonicalCoverKey",
        "seenFacetKeys",
        "CoverKind::EmbeddedCover",
        "BaseNameFromUtf8Path",
    ):
        assert contract in source
    assert '"main.cpp"' not in source
    assert '"WindowsMain.h"' not in source
    assert '"platform/' not in source


def test_main_consumes_cover_facet_item_builder():
    main = MAIN.read_text(encoding="utf-8")
    assert '#include "CoverFacetItemBuilder.h"' in main
    assert "static void BuildCoverWallItemsFromCoverFacets(" not in main
    assert "using aura::cover_wall::BuildCoverWallItemsFromCoverFacets;" in main

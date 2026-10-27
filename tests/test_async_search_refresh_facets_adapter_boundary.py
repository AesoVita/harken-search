from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/AsyncSearchRefreshFacetsAdapter.h"
SOURCE = ROOT / "src/AsyncSearchRefreshFacetsAdapter.cpp"
STATE_SOURCE = ROOT / "src/AsyncSearchRefreshFacetsStateAdapter.cpp"
MAIN = ROOT / "src/AsyncSearchRefreshResultCompositionAdapter.cpp"


def test_refresh_facets_adapter_owns_decision_and_effect_order() -> None:
    assert HEADER.exists()
    assert SOURCE.exists()
    assert STATE_SOURCE.exists()
    assert "class AsyncSearchRefreshFacetsAdapter final" in HEADER.read_text(
        encoding="utf-8"
    )
    source = SOURCE.read_text(encoding="utf-8")
    assert "HandleAsyncSearchRefreshFacets(" in source
    assert "replaceFacets_" in source
    assert "persistFacets_" in source


def test_refresh_facets_adapter_has_one_way_dependencies() -> None:
    combined = "\n".join(
        path.read_text(encoding="utf-8")
        for path in (HEADER, SOURCE, STATE_SOURCE)
    )
    for forbidden in ("main.cpp", "WindowsMain", "src/platform/"):
        assert forbidden not in combined


def test_refresh_facets_root_only_composes_adapter() -> None:
    source = MAIN.read_text(encoding="utf-8")
    body = source
    assert "AppAsyncSearchRefreshFacetsEffects" not in source
    assert "AsyncSearchRefreshFacetsAdapter facetsAdapter(" in body
    assert "facetsAdapter.apply();" in body
    assert "HandleAsyncSearchRefreshFacets(" not in body
    adapter_block = body.split(
        "AsyncSearchRefreshFacetsAdapter facetsAdapter(", 1
    )[1].split("facetsAdapter.apply();", 1)[0]
    assert "state.currentCoverFacets" not in adapter_block
    assert "state.database.persistCoverFacetCache" not in adapter_block

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/AsyncSearchRefreshFacetsWorkflow.h"
SOURCE = ROOT / "src/AsyncSearchRefreshFacetsWorkflow.cpp"
ADAPTER_SOURCE = ROOT / "src/AsyncSearchRefreshFacetsAdapter.cpp"
MAIN = ROOT / "src/AsyncSearchRefreshResultCompositionAdapter.cpp"


def test_refresh_facets_workflow_is_wired_once_and_owns_persistence_order() -> None:
    assert HEADER.exists()
    assert SOURCE.exists()
    header = HEADER.read_text(encoding="utf-8")
    source = SOURCE.read_text(encoding="utf-8")
    main = MAIN.read_text(encoding="utf-8")
    assert "class AsyncSearchRefreshFacetsEffects" in header
    assert "HandleAsyncSearchRefreshFacets(" in source
    assert ADAPTER_SOURCE.exists()
    assert (
        ADAPTER_SOURCE.read_text(encoding="utf-8").count(
            "HandleAsyncSearchRefreshFacets("
        )
        == 1
    )
    apply_body = main
    assert apply_body.count("facetsAdapter.apply();") == 1
    assert "HandleAsyncSearchRefreshFacets(" not in apply_body
    assert "state.database.persistCoverFacetCache" not in apply_body
    assert "state.currentCoverFacets.clear()" not in apply_body
    assert "state.currentPathFacets.clear()" not in apply_body
    assert "shouldApplyRefreshFacets" not in apply_body
    assert "shouldDropStaleFacetsForFallback" not in apply_body
    assert "awaitingFacetFollowUp" not in apply_body

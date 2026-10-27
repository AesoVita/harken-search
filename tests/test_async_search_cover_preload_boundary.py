from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/AsyncSearchCoverPreloadWorkflow.h"
SOURCE = ROOT / "src/AsyncSearchCoverPreloadWorkflow.cpp"
STATE_SOURCE = ROOT / "src/AsyncSearchCoverPreloadStateAdapter.cpp"
MAIN = ROOT / "src/AsyncSearchRefreshResultCompositionAdapter.cpp"


def test_cover_preload_workflow_is_wired_once() -> None:
    assert HEADER.exists()
    assert SOURCE.exists()
    assert "HandleAsyncSearchCoverPreload(" in SOURCE.read_text(encoding="utf-8")
    assert "HandleAsyncSearchCoverPreload(" in STATE_SOURCE.read_text(encoding="utf-8")
    body = MAIN.read_text(encoding="utf-8")
    assert body.count("coverPreloadAdapter.apply();") == 1
    assert "searchResultGlobalNoFilterBrowse" not in body
    assert "deferSearchResultCoverPreloadForCoverWall" not in body

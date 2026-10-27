from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/AsyncSearchRefreshLoadingWorkflow.h"
SOURCE = ROOT / "src/AsyncSearchRefreshLoadingWorkflow.cpp"
STATE_ADAPTER = ROOT / "src/AsyncSearchRefreshLoadingStateAdapter.cpp"
MAIN = ROOT / "src/AsyncSearchRefreshResultCompositionAdapter.cpp"


def test_refresh_loading_workflow_is_wired_once() -> None:
    assert HEADER.exists()
    assert SOURCE.exists()
    assert STATE_ADAPTER.exists()
    assert "HandleAsyncSearchRefreshLoading(" in STATE_ADAPTER.read_text(
        encoding="utf-8"
    )
    body = MAIN.read_text(encoding="utf-8")
    assert body.count("refreshLoadingAdapter.apply();") == 1
    assert "HandleAsyncSearchRefreshLoading(" not in body
    assert "state.loadingMore = false" not in body

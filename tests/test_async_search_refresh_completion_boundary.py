from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/AsyncSearchRefreshCompletionWorkflow.h"
SOURCE = ROOT / "src/AsyncSearchRefreshCompletionWorkflow.cpp"
MAIN = ROOT / "src/AsyncSearchRefreshResultCompositionAdapter.cpp"


def test_refresh_completion_workflow_is_wired_once() -> None:
    assert HEADER.exists()
    assert SOURCE.exists()
    assert "HandleAsyncSearchRefreshCompletion(" in SOURCE.read_text(encoding="utf-8")
    body = MAIN.read_text(encoding="utf-8")
    assert body.count("refreshCompletionAdapter.apply();") == 1
    assert "SearchResultRefreshNeedsCoverWallDisplayRefresh" not in body

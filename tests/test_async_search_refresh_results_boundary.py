from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/AsyncSearchRefreshResultsWorkflow.h"
SOURCE = ROOT / "src/AsyncSearchRefreshResultsWorkflow.cpp"
MAIN = ROOT / "src/AsyncSearchRefreshResultCompositionAdapter.cpp"


def test_refresh_results_workflow_is_wired_once() -> None:
    assert HEADER.exists()
    assert SOURCE.exists()
    assert "HandleAsyncSearchRefreshResults(" in SOURCE.read_text(encoding="utf-8")
    body = MAIN.read_text(encoding="utf-8")
    assert body.count("refreshResultsAdapter.apply();") == 1

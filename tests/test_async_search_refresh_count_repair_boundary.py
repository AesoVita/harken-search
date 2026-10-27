from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/AsyncSearchRefreshCountRepairWorkflow.h"
SOURCE = ROOT / "src/AsyncSearchRefreshCountRepairWorkflow.cpp"
ADAPTER_SOURCE = ROOT / "src/AsyncSearchRefreshCountRepairAdapter.cpp"
MAIN = ROOT / "src/AsyncSearchRefreshResultCompositionAdapter.cpp"


def test_refresh_count_repair_workflow_is_wired_once() -> None:
    assert HEADER.exists()
    assert SOURCE.exists()
    assert "HandleAsyncSearchRefreshCountRepair(" in SOURCE.read_text(encoding="utf-8")
    assert ADAPTER_SOURCE.exists()
    assert (
        ADAPTER_SOURCE.read_text(encoding="utf-8").count(
            "HandleAsyncSearchRefreshCountRepair("
        )
        == 1
    )
    body = MAIN.read_text(encoding="utf-8")
    assert body.count("countRepairAdapter.apply();") == 1
    assert "HandleAsyncSearchRefreshCountRepair(" not in body
    assert "IsFilteredRefreshCountStateImpossibleForCoverWall" not in body

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/AsyncSearchRefreshMemoryWorkflow.h"
SOURCE = ROOT / "src/AsyncSearchRefreshMemoryWorkflow.cpp"
ADAPTER_SOURCE = ROOT / "src/AsyncSearchRefreshMemoryAdapter.cpp"
MAIN = ROOT / "src/AsyncSearchRefreshResultCompositionAdapter.cpp"


def test_refresh_memory_workflow_is_wired_once() -> None:
    assert HEADER.exists()
    assert SOURCE.exists()
    assert "HandleAsyncSearchRefreshMemory(" in SOURCE.read_text(encoding="utf-8")
    assert ADAPTER_SOURCE.exists()
    assert (
        ADAPTER_SOURCE.read_text(encoding="utf-8").count(
            "HandleAsyncSearchRefreshMemory("
        )
        == 1
    )
    body = MAIN.read_text(encoding="utf-8")
    assert body.count("memoryAdapter.apply();") == 1
    assert "HandleAsyncSearchRefreshMemory(" not in body

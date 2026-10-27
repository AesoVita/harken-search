from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/AsyncSearchAppendResultWorkflow.h"
SOURCE = ROOT / "src/AsyncSearchAppendResultWorkflow.cpp"
MAIN = ROOT / "src/AsyncSearchResultApplicationAdapter.cpp"


def test_append_result_workflow_is_wired_through_state_adapter() -> None:
    assert HEADER.exists()
    assert SOURCE.exists()
    main = MAIN.read_text(encoding="utf-8")
    adapter_source = (
        ROOT / "src/AsyncSearchAppendResultStateAdapter.cpp"
    ).read_text(encoding="utf-8")
    assert "HandleAsyncSearchAppendResult(" in adapter_source
    body = main
    assert "appendResultAdapter.apply();" in body
    assert "AppAsyncSearchAppendEffects" not in main
    assert "HandleAsyncSearchAppendResult(" not in body

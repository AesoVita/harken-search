from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/AsyncSearchResultDispatchWorkflow.h"
SOURCE = ROOT / "src/AsyncSearchResultDispatchWorkflow.cpp"
MAIN = ROOT / "src/AsyncSearchResultApplicationAdapter.cpp"


def test_result_dispatch_is_classified_once_before_lifecycle() -> None:
    assert HEADER.exists()
    assert SOURCE.exists()
    assert "ClassifyAsyncSearchResultDispatch(" in SOURCE.read_text(encoding="utf-8")
    body = MAIN.read_text(encoding="utf-8")
    assert body.count("ClassifyAsyncSearchResultDispatch(") == 1
    assert body.index("ClassifyAsyncSearchResultDispatch(") < body.index(
        "lifecycleAdapter.apply()"
    )

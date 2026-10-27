from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/AsyncSearchResultLifecycleWorkflow.h"
SOURCE = ROOT / "src/AsyncSearchResultLifecycleWorkflow.cpp"
ADAPTER = ROOT / "src/AsyncSearchResultLifecycleStateAdapter.cpp"
MAIN = ROOT / "src/AsyncSearchResultApplicationAdapter.cpp"


def test_async_search_result_lifecycle_has_root_adapter_boundary() -> None:
    assert HEADER.exists()
    assert SOURCE.exists()
    header = HEADER.read_text(encoding="utf-8")
    source = SOURCE.read_text(encoding="utf-8")
    adapter = ADAPTER.read_text(encoding="utf-8")
    main = MAIN.read_text(encoding="utf-8")
    body = main

    assert "class AsyncSearchResultLifecycleEffects" in header
    assert "HandleAsyncSearchResultLifecycle(" in source
    assert adapter.count("HandleAsyncSearchResultLifecycle(") == 1
    assert "lifecycleAdapter.apply()" in body
    assert "AppAsyncSearchResultLifecycleEffects" not in main
    assert "state.asyncSearchInProgress = false;" not in body
    assert 'state.filterQueryError = "async search failed";' not in body

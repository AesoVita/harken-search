from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/AsyncSearchRefreshCompletionStateAdapter.h"
SOURCE = ROOT / "src/AsyncSearchRefreshCompletionStateAdapter.cpp"
MAIN = ROOT / "src/main.cpp"


def test_refresh_completion_state_adapter_owns_state_projection_and_effects() -> None:
    assert HEADER.exists()
    assert SOURCE.exists()

    main = MAIN.read_text(encoding="utf-8")
    composition = (
        ROOT / "src" / "AsyncSearchRefreshResultCompositionAdapter.cpp"
    ).read_text(encoding="utf-8")
    assert "AsyncSearchRefreshCompletionStateAdapter" not in main
    assert "refreshCompletionAdapter.apply();" in composition
    assert "class AppAsyncSearchRefreshCompletionEffects" not in main
    assert "HandleAsyncSearchRefreshCompletion(" not in main

    source = SOURCE.read_text(encoding="utf-8")
    assert "HandleAsyncSearchRefreshCompletion(" in source
    assert "requestCoverWallRefresh_(state_)" in source

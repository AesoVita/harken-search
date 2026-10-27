from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src" / "AsyncSearchRefreshResultsStateAdapter.h"
SOURCE = ROOT / "src" / "AsyncSearchRefreshResultsStateAdapter.cpp"
MAIN = ROOT / "src" / "main.cpp"


def test_refresh_results_state_adapter_owns_row_projection_and_effects() -> None:
    assert HEADER.exists()
    assert SOURCE.exists()

    main = MAIN.read_text(encoding="utf-8")
    composition = (
        ROOT / "src" / "AsyncSearchRefreshResultCompositionAdapter.cpp"
    ).read_text(encoding="utf-8")
    assert "AsyncSearchRefreshResultsStateAdapter" not in main
    assert "refreshResultsAdapter.apply();" in composition
    assert "class AppAsyncSearchRefreshResultsEffects" not in main
    assert "HandleAsyncSearchRefreshResults(" not in main

    source = SOURCE.read_text(encoding="utf-8")
    assert "HandleAsyncSearchRefreshResults(" in source
    assert "result_.results" in source

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "src" / "main.cpp"
HEADER = ROOT / "src" / "AsyncSearchResultPumpWorkflow.h"
SOURCE = ROOT / "src" / "AsyncSearchResultPumpWorkflow.cpp"


def test_async_search_result_pump_has_independent_owner():
    assert HEADER.exists()
    assert SOURCE.exists()
    text = SOURCE.read_text(encoding="utf-8")
    assert "namespace aura::search" in text
    assert "Pump(" in text
    assert "PumpOne(" in text
    assert "pollResult" in text
    assert "CancelPending" in text


def test_main_does_not_own_async_search_result_pump_runtime():
    text = MAIN.read_text(encoding="utf-8")
    assert "static void PumpAsyncSearchResults(AppState& state)" not in text
    assert "pollResult(result, expectedId)" not in text
    assert "static void CancelPendingAsyncSearchesForUiMutation(AppState& state)" not in text

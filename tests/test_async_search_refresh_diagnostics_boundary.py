from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/AsyncSearchRefreshDiagnosticsWorkflow.h"
SOURCE = ROOT / "src/AsyncSearchRefreshDiagnosticsWorkflow.cpp"
MAIN = ROOT / "src/AsyncSearchRefreshResultCompositionAdapter.cpp"


def test_refresh_diagnostics_has_pure_decision_and_root_effect_boundary() -> None:
    assert HEADER.exists()
    assert SOURCE.exists()
    header = HEADER.read_text(encoding="utf-8")
    source = SOURCE.read_text(encoding="utf-8")
    body = MAIN.read_text(encoding="utf-8")
    assert "class AsyncSearchRefreshDiagnosticsEffects" in header
    assert "HandleAsyncSearchRefreshDiagnostics(" in source
    assert body.count("refreshDiagnosticsAdapter.apply();") == 1
    assert 'DIAG_METRIC("Search", "RefreshSearchResults"' not in body
    assert 'DIAG_ACTION_FMT("Search.Empty"' not in body

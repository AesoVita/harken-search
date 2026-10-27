from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/AsyncSearchRefreshDiagnosticsStateAdapter.h"
SOURCE = ROOT / "src/AsyncSearchRefreshDiagnosticsStateAdapter.cpp"
MAIN = ROOT / "src/AsyncSearchRefreshResultCompositionAdapter.cpp"


def test_refresh_diagnostics_state_adapter_owns_projection_and_effects() -> None:
    assert HEADER.exists()
    assert SOURCE.exists()

    main = MAIN.read_text(encoding="utf-8")
    body = main
    assert "AsyncSearchRefreshDiagnosticsStateAdapter" in body
    assert "refreshDiagnosticsAdapter.apply();" in body
    assert "class AppAsyncSearchRefreshDiagnosticsEffects" not in main
    assert "HandleAsyncSearchRefreshDiagnostics(" not in body

    source = SOURCE.read_text(encoding="utf-8")
    assert "HandleAsyncSearchRefreshDiagnostics(" in source
    assert 'DIAG_METRIC("Search", "RefreshSearchResults"' in source

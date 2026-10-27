from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/AsyncSearchEmptyFallbackWorkflow.h"
SOURCE = ROOT / "src/AsyncSearchEmptyFallbackWorkflow.cpp"
MAIN = ROOT / "src/AsyncSearchRefreshResultCompositionAdapter.cpp"


def test_empty_fallback_cleanup_has_owned_effect_boundary() -> None:
    assert HEADER.exists()
    assert SOURCE.exists()
    header = HEADER.read_text(encoding="utf-8")
    source = SOURCE.read_text(encoding="utf-8")
    body = MAIN.read_text(encoding="utf-8")
    assert "class AsyncSearchEmptyFallbackEffects" in header
    assert "HandleAsyncSearchEmptyFallback(" in source
    assert body.count("emptyFallbackAdapter.apply();") == 1
    assert "state.coverWall.fallbackItems.clear()" not in body
    assert "state.coverWall.fallbackResultsGen = 0" not in body

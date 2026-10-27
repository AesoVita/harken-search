from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/AsyncSearchEmptyFallbackStateAdapter.h"
SOURCE = ROOT / "src/AsyncSearchEmptyFallbackStateAdapter.cpp"
MAIN = ROOT / "src/AsyncSearchRefreshResultCompositionAdapter.cpp"


def test_empty_fallback_state_adapter_owns_cover_wall_cleanup() -> None:
    assert HEADER.exists()
    assert SOURCE.exists()

    main = MAIN.read_text(encoding="utf-8")
    body = main
    assert "AsyncSearchEmptyFallbackStateAdapter" in body
    assert "emptyFallbackAdapter.apply();" in body
    assert "class AppAsyncSearchEmptyFallbackEffects" not in main
    assert "HandleAsyncSearchEmptyFallback(" not in body

    source = SOURCE.read_text(encoding="utf-8")
    assert "HandleAsyncSearchEmptyFallback(" in source
    assert "fallbackItems.clear()" in source

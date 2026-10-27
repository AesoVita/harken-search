from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/AsyncSearchRefreshMemoryAdapter.h"
SOURCE = ROOT / "src/AsyncSearchRefreshMemoryAdapter.cpp"
STATE_SOURCE = ROOT / "src/AsyncSearchRefreshMemoryStateAdapter.cpp"
MAIN = ROOT / "src/AsyncSearchRefreshResultCompositionAdapter.cpp"


def test_refresh_memory_adapter_owns_projection_and_effect_policy() -> None:
    assert HEADER.exists()
    assert SOURCE.exists()
    header = HEADER.read_text(encoding="utf-8")
    source = SOURCE.read_text(encoding="utf-8")
    assert "class AsyncSearchRefreshMemoryAdapter final" in header
    assert "void apply();" in header
    assert "HandleAsyncSearchRefreshMemory(" in source
    assert "evictLeastRecentlyUsed" in source
    assert "trimFallbackCache" in source


def test_refresh_memory_adapter_has_one_way_dependencies() -> None:
    combined = (
        HEADER.read_text(encoding="utf-8")
        + "\n"
        + SOURCE.read_text(encoding="utf-8")
        + "\n"
        + STATE_SOURCE.read_text(encoding="utf-8")
    )
    for forbidden in (
        "main.cpp",
        "WindowsMain",
        "CoverModule",
        "src/platform/",
    ):
        assert forbidden not in combined


def test_refresh_memory_root_only_composes_adapter() -> None:
    source = MAIN.read_text(encoding="utf-8")
    body = source
    assert "AppAsyncSearchRefreshMemoryEffects" not in source
    assert "AsyncSearchRefreshMemoryStateAdapter memoryAdapter(" in body
    assert "memoryAdapter.apply();" in body
    assert "HandleAsyncSearchRefreshMemory(" not in body
    memory_block = body.split(
        "AsyncSearchRefreshMemoryStateAdapter memoryAdapter(", 1
    )[1].split("memoryAdapter.apply();", 1)[0]
    assert "state.memoryBudget" not in memory_block
    assert "state.waveformCache" not in memory_block

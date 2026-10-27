from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/AsyncSearchRefreshMemoryStateAdapter.h"
SOURCE = ROOT / "src/AsyncSearchRefreshMemoryStateAdapter.cpp"
MAIN = ROOT / "src/AsyncSearchRefreshResultCompositionAdapter.cpp"


def test_memory_state_adapter_owns_root_adapter_construction() -> None:
    assert HEADER.exists()
    assert SOURCE.exists()
    main = MAIN.read_text(encoding="utf-8")
    body = main
    assert "AsyncSearchRefreshMemoryStateAdapter" in body
    assert "AsyncSearchRefreshMemoryAdapter memoryAdapter" not in body
    assert "effects_.releaseWaveformEvicted" in body

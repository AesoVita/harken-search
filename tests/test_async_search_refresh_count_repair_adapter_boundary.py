from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/AsyncSearchRefreshCountRepairAdapter.h"
SOURCE = ROOT / "src/AsyncSearchRefreshCountRepairAdapter.cpp"
STATE_SOURCE = ROOT / "src/AsyncSearchRefreshCountRepairStateAdapter.cpp"
MAIN = ROOT / "src/AsyncSearchRefreshResultCompositionAdapter.cpp"


def test_count_repair_adapter_owns_snapshot_effects_and_total_writeback() -> None:
    assert HEADER.exists()
    assert SOURCE.exists()
    assert STATE_SOURCE.exists()
    header = HEADER.read_text(encoding="utf-8")
    source = SOURCE.read_text(encoding="utf-8")
    assert "class AsyncSearchRefreshCountRepairAdapter final" in header
    assert "void apply();" in header
    assert "HandleAsyncSearchRefreshCountRepair(" in source
    assert "setTotalCount_" in source


def test_count_repair_adapter_has_one_way_dependencies() -> None:
    combined = "\n".join(
        path.read_text(encoding="utf-8")
        for path in (HEADER, SOURCE, STATE_SOURCE)
    )
    for forbidden in ("main.cpp", "WindowsMain", "src/platform/"):
        assert forbidden not in combined


def test_count_repair_root_only_composes_state_adapter() -> None:
    source = MAIN.read_text(encoding="utf-8")
    body = source
    assert "AppAsyncSearchRefreshCountRepairEffects" not in source
    assert "AsyncSearchRefreshCountRepairAdapter countRepairAdapter(" in body
    assert "countRepairAdapter.apply();" in body
    assert "HandleAsyncSearchRefreshCountRepair(" not in body
    adapter_block = body.split(
        "AsyncSearchRefreshCountRepairAdapter countRepairAdapter(", 1
    )[1].split("countRepairAdapter.apply();", 1)[0]
    assert "state.pendingFacetRequestId" not in adapter_block
    assert "state.totalFileCount" not in adapter_block

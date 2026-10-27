from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/AsyncSearchRefreshLoadingStateAdapter.h"
SOURCE = ROOT / "src/AsyncSearchRefreshLoadingStateAdapter.cpp"
MAIN = ROOT / "src/main.cpp"


def test_refresh_loading_state_adapter_owns_loading_writeback() -> None:
    assert HEADER.exists()
    assert SOURCE.exists()
    header = HEADER.read_text(encoding="utf-8")
    source = SOURCE.read_text(encoding="utf-8")
    assert "class AsyncSearchRefreshLoadingStateAdapter final" in header
    assert "void apply();" in header
    assert "HandleAsyncSearchRefreshLoading(" in source
    assert "state_.loadingMore = false;" in source


def test_refresh_loading_state_adapter_has_one_way_dependencies() -> None:
    combined = "\n".join(
        path.read_text(encoding="utf-8") for path in (HEADER, SOURCE)
    )
    for forbidden in ("main.cpp", "WindowsMain", "src/platform/"):
        assert forbidden not in combined


def test_refresh_loading_root_only_composes_state_adapter() -> None:
    source = MAIN.read_text(encoding="utf-8")
    composition = (
        ROOT / "src" / "AsyncSearchRefreshResultCompositionAdapter.cpp"
    ).read_text(encoding="utf-8")
    assert "AppAsyncSearchRefreshLoadingEffects" not in source
    assert "AsyncSearchRefreshLoadingStateAdapter" not in source
    assert "refreshLoadingAdapter.apply();" in composition
    assert "HandleAsyncSearchRefreshLoading(" not in source

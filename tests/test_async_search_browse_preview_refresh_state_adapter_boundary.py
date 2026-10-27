from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/AsyncSearchBrowsePreviewRefreshStateAdapter.h"
SOURCE = ROOT / "src/AsyncSearchBrowsePreviewRefreshStateAdapter.cpp"
MAIN = ROOT / "src/AsyncSearchRefreshResultCompositionAdapter.cpp"


def test_browse_preview_refresh_state_adapter_owns_app_state_effects() -> None:
    assert HEADER.exists()
    assert SOURCE.exists()
    header = HEADER.read_text(encoding="utf-8")
    source = SOURCE.read_text(encoding="utf-8")
    main = MAIN.read_text(encoding="utf-8")
    assert "class AsyncSearchBrowsePreviewRefreshStateAdapter final" in header
    assert "capture()" in header
    assert "restore(" in header
    assert "CaptureAsyncSearchBrowsePreviewRefresh(" in source
    assert "HandleAsyncSearchBrowsePreviewRefreshRestore(" in source
    assert "state_.selectedResultIndex" in source
    assert "AppAsyncSearchBrowsePreviewRefreshEffects" not in main


def test_browse_preview_refresh_state_adapter_has_one_way_dependencies() -> None:
    combined = "\n".join(
        path.read_text(encoding="utf-8") for path in (HEADER, SOURCE)
    )
    for forbidden in ("main.cpp", "WindowsMain", "src/platform/"):
        assert forbidden not in combined


def test_apply_root_only_composes_browse_preview_refresh_adapter() -> None:
    source = MAIN.read_text(encoding="utf-8")
    body = source
    assert "browsePreviewAdapter.capture()" in body
    assert "browsePreviewAdapter.restore(" in body
    assert "AppAsyncSearchBrowsePreviewRefreshEffects" not in body
    assert "CaptureAsyncSearchBrowsePreviewRefresh(" not in body
    assert "HandleAsyncSearchBrowsePreviewRefreshRestore(" not in body

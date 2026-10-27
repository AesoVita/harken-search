from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/AsyncSearchBrowsePreviewWorkflow.h"
SOURCE = ROOT / "src/AsyncSearchBrowsePreviewWorkflow.cpp"
MAIN = ROOT / "src/AsyncSearchRefreshResultCompositionAdapter.cpp"


def test_async_search_browse_preview_has_owned_refresh_boundary() -> None:
    assert HEADER.exists()
    assert SOURCE.exists()
    header = HEADER.read_text(encoding="utf-8")
    source = SOURCE.read_text(encoding="utf-8")
    main = MAIN.read_text(encoding="utf-8")
    body = main

    assert "class AsyncSearchBrowsePreviewEffects" in header
    assert "CaptureAsyncSearchBrowsePreview(" in source
    assert "HandleAsyncSearchBrowsePreviewRestore(" in source
    assert "CaptureAsyncSearchBrowsePreview(" not in body
    assert "HandleAsyncSearchBrowsePreviewRestore(" not in body
    assert body.count("browsePreviewAdapter.capture()") == 1
    assert body.count("browsePreviewAdapter.restore(") == 1
    assert "BrowseSurfacePreviewSelection browseSurfaceSelection" not in body
    assert "RestoreBrowseSurfacePreviewAfterRefresh" not in main

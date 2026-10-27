from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/AsyncSearchBrowsePreviewRefreshWorkflow.h"
SOURCE = ROOT / "src/AsyncSearchBrowsePreviewRefreshWorkflow.cpp"
MAIN = ROOT / "src/AsyncSearchRefreshResultCompositionAdapter.cpp"


def test_browse_preview_refresh_owns_capture_and_restore_projection() -> None:
    assert HEADER.exists()
    assert SOURCE.exists()
    source = SOURCE.read_text(encoding="utf-8")
    assert "CaptureAsyncSearchBrowsePreviewRefresh(" in source
    assert "HandleAsyncSearchBrowsePreviewRefreshRestore(" in source
    body = MAIN.read_text(encoding="utf-8")
    assert body.count("browsePreviewAdapter.capture()") == 1
    assert body.count("browsePreviewAdapter.restore(") == 1
    assert "AsyncSearchBrowsePreviewCaptureInput" not in body
    assert "AsyncSearchBrowsePreviewRestoreInput" not in body
    assert "CanonicalFolderPath(" not in body
    assert "StartDeferredCoverPriming();" not in body
    assert "ApplyCoverKeyFilterInPlace(state);" not in body

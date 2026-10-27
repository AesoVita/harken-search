from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/AsyncSearchCoverPreloadAdapter.h"
SOURCE = ROOT / "src/AsyncSearchCoverPreloadAdapter.cpp"
MAIN = ROOT / "src/AsyncSearchRefreshResultCompositionAdapter.cpp"
STATE_HEADER = ROOT / "src/AsyncSearchCoverPreloadStateAdapter.h"


def test_cover_preload_adapter_owns_folder_dedup_and_record_projection() -> None:
    assert HEADER.exists()
    assert SOURCE.exists()
    assert STATE_HEADER.exists()
    main = MAIN.read_text(encoding="utf-8")
    assert "AsyncSearchCoverPreloadStateAdapter" in main
    body = main
    assert "class AppAsyncSearchCoverPreloadEffects" not in main
    assert "CoverModule::PreCacheFolderCovers" not in body
    assert "folders_" not in body

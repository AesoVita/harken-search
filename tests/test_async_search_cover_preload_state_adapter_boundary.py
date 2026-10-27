from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/AsyncSearchCoverPreloadStateAdapter.h"
SOURCE = ROOT / "src/AsyncSearchCoverPreloadStateAdapter.cpp"
MAIN = ROOT / "src/AsyncSearchRefreshResultCompositionAdapter.cpp"


def test_cover_preload_state_adapter_owns_root_projection_and_cover_effects() -> None:
    assert HEADER.exists()
    assert SOURCE.exists()
    main = MAIN.read_text(encoding="utf-8")
    body = main
    assert "AsyncSearchCoverPreloadStateAdapter" in body
    assert "CoverModule::PreCacheFolderCovers" not in body
    assert "CoverModule::GetEmbeddedCoverTexture" not in body
    assert "IsCoverKeyPendingOrFailed" not in body

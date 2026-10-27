from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/AsyncSearchPlaylistResultPreparationStateAdapter.h"
SOURCE = ROOT / "src/AsyncSearchPlaylistResultPreparationStateAdapter.cpp"
MAIN = ROOT / "src/AsyncSearchResultApplicationAdapter.cpp"


def test_playlist_result_preparation_state_adapter_owns_projection_and_effects() -> None:
    assert HEADER.exists()
    assert SOURCE.exists()

    main = MAIN.read_text(encoding="utf-8")
    body = main
    assert "AsyncSearchPlaylistResultPreparationStateAdapter" in body
    assert "playlistPreparationAdapter.apply();" in body
    assert "class AppAsyncSearchPlaylistResultPreparationEffects" not in main
    assert "HandleAsyncSearchPlaylistResultPreparation(" not in body
    assert "playlistPreparation.loadedCount" not in body

    source = SOURCE.read_text(encoding="utf-8")
    assert "HandleAsyncSearchPlaylistResultPreparation(" in source
    assert "result_.loadedCount = preparation.loadedCount;" in source

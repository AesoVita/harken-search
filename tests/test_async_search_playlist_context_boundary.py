from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/AsyncSearchPlaylistContextWorkflow.h"
SOURCE = ROOT / "src/AsyncSearchPlaylistContextWorkflow.cpp"
STATE_SOURCE = ROOT / "src/AsyncSearchPlaylistContextStateAdapter.cpp"
MAIN = ROOT / "src/AsyncSearchResultApplicationAdapter.cpp"


def test_async_search_playlist_context_has_root_effect_boundary() -> None:
    assert HEADER.exists()
    assert SOURCE.exists()
    header = HEADER.read_text(encoding="utf-8")
    source = SOURCE.read_text(encoding="utf-8")
    state_source = STATE_SOURCE.read_text(encoding="utf-8")
    main = MAIN.read_text(encoding="utf-8")
    body = main

    assert "class AsyncSearchPlaylistContextEffects" in header
    assert "HandleAsyncSearchPlaylistContext(" in source
    assert state_source.count("HandleAsyncSearchPlaylistContext(") == 1
    assert body.count("playlistContextAdapter.apply();") == 1
    assert "state.playlistAggregatedView = result.playlistAggregatedView;" not in body
    assert "state.selectedPlaylistId = result.playlistId;" not in body

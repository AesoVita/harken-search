from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/AsyncSearchPlaylistResultPreparationWorkflow.h"
SOURCE = ROOT / "src/AsyncSearchPlaylistResultPreparationWorkflow.cpp"
MAIN = ROOT / "src/AsyncSearchResultApplicationAdapter.cpp"


def test_playlist_result_preparation_is_shared_by_refresh_and_append() -> None:
    assert HEADER.exists()
    assert SOURCE.exists()
    assert "HandleAsyncSearchPlaylistResultPreparation(" in SOURCE.read_text(
        encoding="utf-8"
    )
    main = MAIN.read_text(encoding="utf-8")
    body = main
    assert body.count("AsyncSearchPlaylistResultPreparationStateAdapter") == 2
    call = body.index("playlistPreparationAdapter.apply();")
    refresh_branch = body.index(
        "dispatch == AsyncSearchResultDispatch::Refresh"
    )
    assert call < refresh_branch
    assert "ApplyPlaylistScopedFilters(" not in body

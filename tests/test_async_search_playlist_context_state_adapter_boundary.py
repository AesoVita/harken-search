from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/AsyncSearchPlaylistContextStateAdapter.h"
SOURCE = ROOT / "src/AsyncSearchPlaylistContextStateAdapter.cpp"
MAIN = ROOT / "src/AsyncSearchResultApplicationAdapter.cpp"


def test_playlist_context_state_adapter_owns_context_projection_and_effects() -> None:
    assert HEADER.exists()
    assert SOURCE.exists()
    main = MAIN.read_text(encoding="utf-8")
    body = main
    assert "AsyncSearchPlaylistContextStateAdapter" in body
    assert "class AppAsyncSearchPlaylistContextEffects" not in main

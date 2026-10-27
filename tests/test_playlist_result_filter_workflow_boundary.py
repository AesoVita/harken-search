from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/PlaylistResultFilterWorkflow.h"
SOURCE = ROOT / "src/PlaylistResultFilterWorkflow.cpp"
MAIN = ROOT / "src/main.cpp"
EFFECT_FACTORY = ROOT / "src/AsyncSearchResultEffectFactory.cpp"


def test_playlist_result_filter_has_pure_filter_and_root_effect_boundary() -> None:
    assert HEADER.exists()
    assert SOURCE.exists()
    header = HEADER.read_text(encoding="utf-8")
    source = SOURCE.read_text(encoding="utf-8")
    main = MAIN.read_text(encoding="utf-8")
    adapter = (ROOT / "src/AsyncSearchResultFilterAdapter.cpp").read_text(
        encoding="utf-8"
    )
    effect_factory = EFFECT_FACTORY.read_text(encoding="utf-8")
    assert "class PlaylistResultFilterEffects" in header
    assert "HandlePlaylistResultFilter(" in source
    assert "aura::playlist::HandlePlaylistResultFilter(" in adapter
    assert "ApplyPlaylistScopedFilters," in effect_factory
    assert "FilterPlaylistRecordsByFolderInPlace" not in main

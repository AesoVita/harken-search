from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "src/main.cpp"
HEADER = ROOT / "src/SearchPresetFilterBuilders.h"
SOURCE = ROOT / "src/SearchPresetFilterBuilders.cpp"


def test_preset_filter_builders_have_independent_owner():
    assert HEADER.exists()
    assert SOURCE.exists()
    text = SOURCE.read_text(encoding="utf-8")
    for symbol in (
        "BuildCurrentPlaylistSearchFilter",
        "BuildCurrentSmartCollectionFilter",
        "ApplyFileTypePolicyToFilter",
    ):
        assert symbol in text
    assert "BuildCurrentFavoritesFilter" not in text


def test_main_does_not_define_preset_filter_builders():
    text = MAIN.read_text(encoding="utf-8")
    for symbol in (
        "static SearchFilter BuildCurrentFavoritesFilter(",
        "static SearchFilter BuildCurrentPlaylistSearchFilter(",
        "static SearchFilter BuildCurrentSmartCollectionFilter(",
    ):
        assert symbol not in text
    assert "BuildCurrentFavoritesFilter" not in text

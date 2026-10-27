from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "src" / "main.cpp"
HEADER = ROOT / "src" / "AsyncSearchResultEffectFactory.h"
SOURCE = ROOT / "src" / "AsyncSearchResultEffectFactory.cpp"


def test_effect_factory_owns_fixed_application_effect_topology():
    assert HEADER.exists()
    assert SOURCE.exists()
    text = SOURCE.read_text(encoding="utf-8")
    for owned in (
        "ApplyPlaylistScopedFilters",
        "FilterResultsByFileTypePolicy",
        "StartDeferredCoverPriming",
        "SubmitCountAndFacetRepair",
        "ScheduleDeferredDetail",
        "MarkFilePlayed",
    ):
        assert owned in text


def test_main_only_injects_root_specific_effect_ports():
    text = MAIN.read_text(encoding="utf-8")
    body = text.split("BuildAsyncSearchResultApplicationEffects() {", 1)[1].split(
        "\n}", 1
    )[0]
    assert "BuildApplicationEffects" in body
    assert "ApplyPlaylistScopedFilters" not in body
    assert "SubmitCountAndFacetRepair" not in body

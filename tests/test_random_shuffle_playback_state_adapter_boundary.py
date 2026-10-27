from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src" / "RandomShufflePlaybackStateAdapter.h"
SOURCE = ROOT / "src" / "RandomShufflePlaybackStateAdapter.cpp"
MAIN = ROOT / "src" / "main.cpp"
COMPOSITION = ROOT / "src" / "AsyncSearchRefreshResultCompositionAdapter.cpp"


def test_random_shuffle_state_adapter_owns_result_projection_and_effects() -> None:
    assert HEADER.exists()
    assert SOURCE.exists()

    main = MAIN.read_text(encoding="utf-8")
    root_body = main.split("BuildAsyncSearchResultApplicationEffects()", 1)[1].split(
        "bool PumpOneAsyncSearchResult", 1
    )[0]
    composition = COMPOSITION.read_text(encoding="utf-8")
    assert "RandomShufflePlaybackStateAdapter" in composition
    assert "randomShuffleAdapter.apply();" in composition
    assert "class AppRandomShufflePlaybackEffects" not in main
    assert "HandleRandomShufflePlayback(" not in root_body
    assert "randomShuffleFirst" not in root_body

    source = SOURCE.read_text(encoding="utf-8")
    assert "HandleRandomShufflePlayback(" in source
    assert "result_.isRandomShuffle" in source

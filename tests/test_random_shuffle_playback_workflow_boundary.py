from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src" / "RandomShufflePlaybackWorkflow.h"
SOURCE = ROOT / "src" / "RandomShufflePlaybackWorkflow.cpp"
MAIN = ROOT / "src" / "main.cpp"
COMPOSITION = ROOT / "src" / "AsyncSearchRefreshResultCompositionAdapter.cpp"


def test_random_shuffle_playback_workflow_has_effect_port_and_root_delegation() -> None:
    assert HEADER.exists()
    assert SOURCE.exists()
    header = HEADER.read_text(encoding="utf-8")
    source = SOURCE.read_text(encoding="utf-8")
    main = MAIN.read_text(encoding="utf-8")
    assert "class RandomShufflePlaybackEffects" in header
    assert "HandleRandomShufflePlayback(" in header
    assert "loadFileAsync" in source
    composition = COMPOSITION.read_text(encoding="utf-8")
    assert composition.count("randomShuffleAdapter.apply();") == 1
    assert "if (result.isRandomShuffle" not in composition

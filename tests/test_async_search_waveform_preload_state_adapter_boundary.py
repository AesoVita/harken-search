from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/AsyncSearchWaveformPreloadStateAdapter.h"
SOURCE = ROOT / "src/AsyncSearchWaveformPreloadStateAdapter.cpp"
MAIN = ROOT / "src/AsyncSearchRefreshResultCompositionAdapter.cpp"


def test_waveform_preload_state_adapter_owns_state_projection_and_enqueue_effect() -> None:
    assert HEADER.exists()
    assert SOURCE.exists()
    header = HEADER.read_text(encoding="utf-8")
    source = SOURCE.read_text(encoding="utf-8")
    assert "class AsyncSearchWaveformPreloadStateAdapter final" in header
    assert "void apply();" in header
    assert "HandleAsyncSearchWaveformPreload(" in source
    assert "state_.addPendingWaveform(record.filePath, true);" in source


def test_waveform_preload_state_adapter_has_one_way_dependencies() -> None:
    combined = "\n".join(
        path.read_text(encoding="utf-8") for path in (HEADER, SOURCE)
    )
    for forbidden in ("main.cpp", "WindowsMain", "src/platform/"):
        assert forbidden not in combined


def test_waveform_preload_root_only_composes_state_adapter() -> None:
    source = MAIN.read_text(encoding="utf-8")
    body = source
    assert "AppAsyncSearchWaveformPreloadEffects" not in source
    assert "AsyncSearchWaveformPreloadStateAdapter" in body
    assert "waveformPreloadAdapter(" in body
    assert "waveformPreloadAdapter.apply();" in body
    assert "HandleAsyncSearchWaveformPreload(" not in body
    adapter_block = body.split("AsyncSearchWaveformPreloadStateAdapter", 1)[1].split(
        "waveformPreloadAdapter.apply();", 1
    )[0]
    assert "state.searchResults" not in adapter_block
    assert "state.dbSwitchSettleFramesRemaining" not in adapter_block

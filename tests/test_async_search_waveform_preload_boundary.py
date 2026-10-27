from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/AsyncSearchWaveformPreloadWorkflow.h"
SOURCE = ROOT / "src/AsyncSearchWaveformPreloadWorkflow.cpp"
STATE_ADAPTER = ROOT / "src/AsyncSearchWaveformPreloadStateAdapter.cpp"
MAIN = ROOT / "src/AsyncSearchRefreshResultCompositionAdapter.cpp"


def test_waveform_preload_workflow_is_wired_once() -> None:
    assert HEADER.exists()
    assert SOURCE.exists()
    assert STATE_ADAPTER.exists()
    assert "HandleAsyncSearchWaveformPreload(" in SOURCE.read_text(encoding="utf-8")
    adapter = STATE_ADAPTER.read_text(encoding="utf-8")
    body = MAIN.read_text(encoding="utf-8")
    assert adapter.count("HandleAsyncSearchWaveformPreload(") == 1
    assert "HandleAsyncSearchWaveformPreload(" not in body
    assert body.count("waveformPreloadAdapter.apply();") == 1

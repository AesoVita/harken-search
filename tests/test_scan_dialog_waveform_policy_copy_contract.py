from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
CMAKE = (ROOT / "cmake/harken_search_behavior_tests.cmake").read_text(encoding="utf-8")


def _scan_setup_dialog() -> str:
    start = MAIN.index("// Scan folder dialog")
    end = MAIN.index("auto startScanForFolders", start)
    return MAIN[start:end]


def test_scan_dialog_omits_library_waveform_policy_copy() -> None:
    assert "TestScanDialogWaveformPolicyCopyContract" in CMAKE
    dialog = _scan_setup_dialog()
    assert "const auto scanCacheMode" not in dialog
    assert "state.database.getWaveformCacheMode()" not in dialog


def test_scan_strategy_controls_remain_intact() -> None:
    dialog = _scan_setup_dialog()
    assert "const bool fastScanMode" in dialog
    assert "state.settings.cacheWaveformsOnScan = false;" in dialog
    assert "state.settings.cacheWaveformsOnScan = true;" in dialog

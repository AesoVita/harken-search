from pathlib import Path
import json

ROOT = Path(__file__).resolve().parents[1]

def test_waveform_view_reset_adapter_owns_reset_logic_and_root_only_wires():
    header = (ROOT / "src/WaveformViewResetStateAdapter.h").read_text()
    source = (ROOT / "src/WaveformViewResetStateAdapter.cpp").read_text()
    main = (ROOT / "src/main.cpp").read_text()
    assert "ResetRegionEditor" in header and "ResetZoom" in header
    assert "ExitTrimRefine" in source
    assert "ResetWaveformRegionEditorState" not in main
    assert "static void ExitWaveformTrimRefine" not in main
    assert "void ResetWaveformZoom(AppState& state);" not in main
    assert '#include "WaveformViewResetStateAdapter.h"' in main

def test_waveform_view_reset_adapter_has_no_forbidden_reverse_dependencies():
    for path in [ROOT / "src/WaveformViewResetStateAdapter.h",
                 ROOT / "src/WaveformViewResetStateAdapter.cpp"]:
        text = path.read_text()
        assert "main.cpp" not in text
        assert "WindowsMain" not in text
        assert "platform/" not in text

def test_waveform_view_reset_adapter_is_governed():
    cmake = (ROOT / "CMakeLists.txt").read_text()
    rules = json.loads((ROOT / "docs/architecture/stage-4-dependency-rules.json").read_text())
    stage7 = (ROOT / "tools/stage7_api_contract_audit.py").read_text()
    behavior = (ROOT / "cmake/harken_search_behavior_tests.cmake").read_text()
    assert "WaveformViewResetStateAdapter.cpp" in cmake
    assert "src/WaveformViewResetStateAdapter.cpp" in rules["scope"]
    assert "WaveformViewResetStateAdapter.h" in stage7
    assert "TestWaveformViewResetStateAdapterBoundary" in behavior

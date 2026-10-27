from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
FACTORY = (ROOT / "src" / "AsyncSearchResultEffectFactory.cpp").read_text(encoding="utf-8")
APP_STATE = (ROOT / "src" / "AppState.h").read_text(encoding="utf-8")
HEADER_PATH = ROOT / "src" / "WaveformDetailSchedulingStateAdapter.h"
SOURCE_PATH = ROOT / "src" / "WaveformDetailSchedulingStateAdapter.cpp"
CMAKE = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
BEHAVIOR = (ROOT / "cmake" / "harken_search_behavior_tests.cmake").read_text(
    encoding="utf-8"
)
RULES = (
    ROOT / "docs" / "architecture" / "stage-4-dependency-rules.json"
).read_text(encoding="utf-8")
STAGE7 = (ROOT / "tools" / "stage7_api_contract_audit.py").read_text(
    encoding="utf-8"
)


def test_waveform_detail_scheduling_adapter_owns_request_state_machine():
    assert HEADER_PATH.exists()
    assert SOURCE_PATH.exists()

    header = HEADER_PATH.read_text(encoding="utf-8")
    source = SOURCE_PATH.read_text(encoding="utf-8")

    assert "namespace aura::waveform" in header
    assert "void ScheduleDeferredDetail(" in header
    assert '#include "AppState.h"' in source
    assert '#include "WaveformPresentationPolicy.h"' in source
    assert '#include "imgui.h"' in source
    assert "state.pendingDetailFilePath = filePath;" in source
    assert "state.pendingDetailTime = ImGui::GetTime();" in source
    assert "NormalizeMainWaveformRequestPeaks(" in source
    assert "if (!filePath.empty() && !state.waveformRebuildActive)" in source
    assert "state.waveformWorker.cancelAllPending();" in source
    assert "state.meterFollowupPending.clear();" in source
    assert "state.waveformFailed.erase(filePath);" in source
    assert "state.waveformMiniCacheMiss.erase(filePath);" in source
    assert "state.visibleFilePaths.insert(filePath);" in source
    assert "state.addPendingWaveform(filePath, true);" in source

    for forbidden in ("main.cpp", "WindowsMain", "platform/"):
        assert forbidden not in header
        assert forbidden not in source


def test_global_schedule_abi_remains_below_root_and_result_effect_is_direct():
    source = SOURCE_PATH.read_text(encoding="utf-8")

    assert (
        "void ScheduleDeferredWaveformDetail(AppState& state, "
        "const std::string& filePath, int targetPeaks);"
        in APP_STATE
    )
    assert "void ScheduleDeferredWaveformDetail(AppState& state," in source
    assert "aura::waveform::ScheduleDeferredDetail(" in source

    assert '#include "WaveformDetailSchedulingStateAdapter.h"' in MAIN
    assert "void ScheduleDeferredWaveformDetail(AppState& state," not in MAIN
    assert FACTORY.count("ScheduleDeferredDetail,") == 1


def test_waveform_detail_scheduling_adapter_is_built_tested_and_governed():
    assert CMAKE.count("src/WaveformDetailSchedulingStateAdapter.cpp") == 1
    assert BEHAVIOR.count(
        "tests/test_waveform_detail_scheduling_state_adapter_boundary.py"
    ) == 1
    assert RULES.count("src/WaveformDetailSchedulingStateAdapter.cpp") >= 2
    assert RULES.count("src/WaveformDetailSchedulingStateAdapter.h") >= 2
    assert (
        '"src/WaveformDetailSchedulingStateAdapter.h": '
        '"tests/test_waveform_detail_scheduling_state_adapter_boundary.py"'
        in STAGE7
    )

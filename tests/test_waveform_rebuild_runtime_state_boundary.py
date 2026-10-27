from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER_PATH = ROOT / "src/WaveformRebuildRuntimeState.h"
SOURCE_PATH = ROOT / "src/WaveformRebuildRuntimeState.cpp"

assert HEADER_PATH.is_file(), f"missing header: {HEADER_PATH}"
assert SOURCE_PATH.is_file(), f"missing source: {SOURCE_PATH}"

HEADER = HEADER_PATH.read_text(encoding="utf-8")
SOURCE = SOURCE_PATH.read_text(encoding="utf-8")
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
RECOVERY_ADAPTER = (
    ROOT / "src/WaveformRebuildCancellationRecoveryStateAdapter.cpp"
).read_text(encoding="utf-8")
CMAKE = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
BEHAVIOR = (ROOT / "cmake/harken_search_behavior_tests.cmake").read_text(encoding="utf-8")

assert "struct AppState;" in HEADER
assert "namespace aura::waveform_rebuild" in HEADER
assert "class WaveformRebuildRuntimeState" in HEADER
for token in (
    "void Reset(AppState& state, bool cancelWorkerQueue);",
    "bool batchMiniOnly() const noexcept;",
    "void setBatchMiniOnly(bool value) noexcept;",
    "bool incrementalMiniOnly() const noexcept;",
    "void setIncrementalMiniOnly(bool value) noexcept;",
):
    assert token in HEADER, token

assert MAIN.count("g_waveformRebuildRuntimeState;") == 1
assert MAIN.count("g_waveformRebuildRuntimeState.Reset(") == 4
assert MAIN.count("g_waveformRebuildRuntimeState.Reset(state,") == 4
assert "runtimeState_.Reset(state_, false);" in RECOVERY_ADAPTER
for removed in (
    "ResetWaveformRebuildRuntimeState",
    "g_waveformRebuildBatchMiniOnly",
    "g_waveformRebuildIncrementalMiniOnly",
):
    assert removed not in MAIN, removed

assert CMAKE.count("src/WaveformRebuildRuntimeState.cpp") == 1
assert BEHAVIOR.count("src/WaveformRebuildRuntimeState.cpp") == 1
assert "NAME TestWaveformRebuildRuntimeStateBoundary" in BEHAVIOR

for owned in (
    "state.waveformRebuildActive = false;",
    "state.waveformRebuildFullMode = false;",
    "state.waveformRebuildTotal = 0;",
    "state.waveformRebuildMiniTotal = 0;",
    "state.waveformRebuildDetailTotal = 0;",
    "state.waveformRebuildProgress = 0;",
    "state.waveformRebuildSkipped = 0;",
    "state.waveformRebuildFailed = 0;",
    "state.waveformRebuildCancel = false;",
    "state.waveformRebuildOffset = 0;",
    "state.waveformRebuildBatch.clear();",
    "state.waveformRebuildQueuedIds.clear();",
    "state.waveformRebuildBatchIdx = 0;",
    "state.clearPendingWaveforms(cancelWorkerQueue);",
    "state.waveformRebuildMiniOnlyMode = false;",
    "batchMiniOnly_ = false;",
    "incrementalMiniOnly_ = false;",
):
    assert owned in SOURCE, owned

for private_state in (
    "bool batchMiniOnly_ = false;",
    "bool incrementalMiniOnly_ = false;",
):
    assert private_state in HEADER, private_state

for forbidden in (
    "Database.h",
    "AsyncWorker.h",
    "CoverModule.h",
    "Platform::",
    "glDelete",
    "OpenGL",
    "main.cpp",
    "WindowsMain",
):
    assert forbidden not in HEADER, forbidden
    assert forbidden not in SOURCE, forbidden

print("PASS: waveform rebuild runtime state boundary contracts")

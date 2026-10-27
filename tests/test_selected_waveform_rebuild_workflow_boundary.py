from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER_PATH = ROOT / "src/SelectedWaveformRebuildWorkflow.h"
SOURCE_PATH = ROOT / "src/SelectedWaveformRebuildWorkflow.cpp"

assert HEADER_PATH.is_file(), f"missing header: {HEADER_PATH}"
assert SOURCE_PATH.is_file(), f"missing source: {SOURCE_PATH}"

HEADER = HEADER_PATH.read_text(encoding="utf-8")
SOURCE = SOURCE_PATH.read_text(encoding="utf-8")
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
CMAKE = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
BEHAVIOR = (ROOT / "cmake/harken_search_behavior_tests.cmake").read_text(encoding="utf-8")


def function_body(source: str, signature: str) -> str:
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[brace + 1 : index]
    raise AssertionError(f"unterminated function: {signature}")


for token in (
    "enum class SelectedWaveformRebuildPumpDecision",
    "struct SelectedWaveformRebuildStats",
    "struct SelectedWaveformRebuildPumpRequest",
    "struct SelectedWaveformRebuildPumpResult",
    "class SelectedWaveformRebuildStoragePort",
    "class SelectedWaveformRebuildEffectsPort",
    "class SelectedWaveformRebuildWorkflow",
):
    assert token in HEADER, token

for token in (
    "constexpr std::size_t kPerFrame = 4",
    "queue_.pump(kPerFrame",
    "effects.loadedFilePath()",
    "storage.miniOnlyPolicy()",
    "SubmitWaveformRebuildRequest(",
    "effects.addPendingWaveform(item.filePath, !miniOnly)",
    "effects.reportTransactionCommitFailure()",
    "effects.reportCompleted(result.stats)",
):
    assert token in SOURCE, token

for forbidden in (
    "AppState",
    "Database.h",
    "AudioEngine",
    "WaveformWorker",
    "DiagnosticsLogger",
    "DIAG_",
    "ImGui",
    "Platform::",
    "main.cpp",
    "WindowsMain",
):
    assert forbidden not in HEADER, forbidden
    assert forbidden not in SOURCE, forbidden

pump = function_body(MAIN, "static void PumpSelectedWaveformRebuild(AppState& state)")
start = function_body(
    MAIN,
    "static void RebuildWaveformsForSelected(AppState& state, const std::vector<AudioRecord>& records)",
)
explicit = function_body(
    MAIN,
    "static bool StartWaveformRebuild(AppState& state,",
)
current = function_body(MAIN, "static void RebuildCurrentFileMainWaveform(AppState& state)")
WINMAIN = (ROOT / "src/WindowsMain.cpp").read_text(encoding="utf-8")

assert MAIN.count("g_selectedWaveformRebuildWorkflow") >= 4
assert pump.count("g_selectedWaveformRebuildWorkflow.pump(") == 1
for removed in (
    "g_selectedWaveformRebuildQueue",
    "g_selectedWaveformRebuildCleared",
    "g_selectedWaveformRebuildQueued",
    "g_selectedWaveformRebuildPending",
    "SELECTED_WAVEFORM_REBUILD_PER_FRAME",
):
    assert removed not in MAIN, removed

for removed_from_pump in (
    "beginTransaction(",
    "clearWaveformCacheByPath(",
    "commitTransaction(",
    "rollbackTransaction(",
    "getLoadedFilePath(",
    "getWaveformCacheMode(",
    "ClearWaveformUiCachesForPath(",
    "SubmitWaveformRebuildRequest(",
    "addPendingWaveform(",
    "forceRegenerateWaveform(",
):
    assert removed_from_pump not in pump, removed_from_pump

assert "g_selectedWaveformRebuildWorkflow.start(items)" in start
assert "g_selectedWaveformRebuildWorkflow.remaining()" in start
assert "const int rebuildThreads = 2" in start
assert "state.waveformWorker.start(&state.database, rebuildThreads)" in start
assert "state.waveformWorker.ensureThreadCount(rebuildThreads)" in start
assert "g_selectedWaveformRebuildWorkflow.clear()" in explicit

for forbidden_from_click in (
    "clearWaveformCacheByPath(",
    "upsertWaveformCacheStateByPath(",
    "forceRegenerateWaveform(",
    "submitDetailRequest(",
):
    assert forbidden_from_click not in current, forbidden_from_click

assert "queueCurrentFile(" in current
assert "PathExistsUtf8(" in current
assert "g_selectedWaveformRebuildWorkflow.cancel()" in MAIN
assert "正在重建当前文件波形" in MAIN
assert "g_selectedWaveformRebuildWorkflow.observeWorkerResult(" in MAIN
assert "void ShutdownSelectedWaveformRebuild()" in MAIN
assert "g_selectedWaveformRebuildWorkflow.shutdown()" in MAIN
assert "ShutdownSelectedWaveformRebuild();" in WINMAIN
assert "g_selectedWaveformRebuildWorkflow" not in WINMAIN

for token in (
    "enum class SelectedWaveformRebuildQueueDecision",
    "enum class SelectedWaveformRebuildLifecycle",
    "queueCurrentFile(",
    "currentFileTaskActive(",
    "observeWorkerResult(",
    "shutdown()",
):
    assert token in HEADER, token

assert CMAKE.count("src/SelectedWaveformRebuildWorkflow.cpp") == 1
assert BEHAVIOR.count("src/SelectedWaveformRebuildWorkflow.cpp") == 1
assert "TestSelectedWaveformRebuildWorkflow" in BEHAVIOR
assert "NAME TestSelectedWaveformRebuildWorkflowBoundary" in BEHAVIOR

print("PASS: selected waveform rebuild workflow boundary contracts")

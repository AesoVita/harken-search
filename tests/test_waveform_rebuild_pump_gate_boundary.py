from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER_PATH = ROOT / "src/WaveformRebuildPumpGate.h"
SOURCE_PATH = ROOT / "src/WaveformRebuildPumpGate.cpp"

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
    "enum class WaveformRebuildBatchGateDecision",
    "struct WaveformRebuildCapacityGateInput",
    "struct WaveformRebuildCapacityGateResult",
    "struct WaveformRebuildBatchGateInput",
    "EvaluateWaveformRebuildCapacityGate(",
    "ShouldSampleWaveformRebuildDrain(",
    "EvaluateWaveformRebuildBatchGate(",
):
    assert token in HEADER, token

for owned in (
    "input.urgentPending + input.normalPending + input.workerPending",
    "pending >= input.queueThreshold",
    "input.batchIndex < static_cast<int>(input.batchSize)",
    "input.progress > 0 || input.offset > 0",
    "bool ShouldSampleWaveformRebuildDrain(",
    "WaveformRebuildBatchGateDecision::WaitForDrain",
    "WaveformRebuildBatchGateDecision::LoadBatch",
):
    assert owned in SOURCE, owned

body = function_body(MAIN, "static void PumpWaveformRebuild(AppState& state)")
assert MAIN.count("EvaluateWaveformRebuildCapacityGate(") == 1
assert MAIN.count("ShouldSampleWaveformRebuildDrain(") == 1
assert MAIN.count("EvaluateWaveformRebuildBatchGate(") == 1
for removed in (
    "size_t pending = state.waveformUrgentPending.size()",
    "pending >= WAVEFORM_REBUILD_QUEUE_THRESHOLD",
    "const bool alreadySubmittedRebuildWork",
    "alreadySubmittedRebuildWork && state.waveformWorker.pendingCount() > 0",
):
    assert removed not in body, removed
for applied in (
    "size_t pending = capacityGate.pending",
    "if (capacityGate.wait) return",
    "WaveformRebuildBatchGateDecision::WaitForDrain",
    "WaveformRebuildBatchGateDecision::LoadBatch",
    "ShouldSampleWaveformRebuildDrain(batchGateInput)",
):
    assert applied in body, applied

prefix = body[: body.index("LoadWaveformRebuildBatchLifecycle(")]
assert prefix.count("state.waveformWorker.pendingCount()") == 2
first_sample = prefix.index("state.waveformWorker.pendingCount()")
capacity_call = prefix.index("EvaluateWaveformRebuildCapacityGate(")
capacity_return = prefix.index("if (capacityGate.wait) return")
sampling_guard = prefix.index("ShouldSampleWaveformRebuildDrain(batchGateInput)")
second_sample = prefix.rindex("state.waveformWorker.pendingCount()")
batch_call = prefix.index("EvaluateWaveformRebuildBatchGate(")
assert first_sample < capacity_call < capacity_return < sampling_guard
assert sampling_guard < second_sample < batch_call

assert CMAKE.count("src/WaveformRebuildPumpGate.cpp") == 1
assert BEHAVIOR.count("src/WaveformRebuildPumpGate.cpp") == 1
assert "TestWaveformRebuildPumpGate" in BEHAVIOR
assert "NAME TestWaveformRebuildPumpGateBoundary" in BEHAVIOR

for forbidden in (
    "AppState.h",
    "AsyncWorker.h",
    "Database.h",
    "DiagnosticsLogger",
    "ModuleUI.h",
    "Platform::",
    "WaveformRebuildRuntimeState.h",
    "glDelete",
    "main.cpp",
    "WindowsMain",
):
    assert forbidden not in HEADER, forbidden
    assert forbidden not in SOURCE, forbidden

print("PASS: waveform rebuild pump gate boundary contracts")

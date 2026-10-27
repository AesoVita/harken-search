from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER_PATH = ROOT / "src/WaveformRebuildFailurePersistence.h"
SOURCE_PATH = ROOT / "src/WaveformRebuildFailurePersistence.cpp"

assert HEADER_PATH.is_file(), f"missing header: {HEADER_PATH}"
assert SOURCE_PATH.is_file(), f"missing source: {SOURCE_PATH}"

HEADER = HEADER_PATH.read_text(encoding="utf-8")
SOURCE = SOURCE_PATH.read_text(encoding="utf-8")
WORKFLOW = (ROOT / "src/WaveformRebuildRecordWorkflow.cpp").read_text(
    encoding="utf-8"
)
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
    "struct WaveformRebuildFailureRequest",
    "class WaveformRebuildFailureSink",
    "class DatabaseWaveformRebuildFailureSink final",
    "bool PersistPreviousWaveformRebuildFailure(",
):
    assert token in HEADER, token

for owned in (
    "request.fullMode",
    "request.previouslyFailed",
    '"foreground_mini_rebuild_previous_failure"',
    '"foreground_waveform_rebuild_previous_failure"',
    "sink.persistPreviousFailure(request.record, reason)",
    "WaveformExtractor::BuildCacheKey(",
    "database_.upsertWaveformCacheStateByPath(",
):
    assert owned in SOURCE, owned

pump = function_body(MAIN, "static void PumpWaveformRebuild(AppState& state)")
assert MAIN.count("PersistPreviousWaveformRebuildFailure(") == 0
assert WORKFLOW.count("PersistPreviousWaveformRebuildFailure(") == 1
assert "DatabaseWaveformRebuildFailureSink failureSink(" in pump
assert "state.database);" in pump
assert "++result.state.failed;" in WORKFLOW
for removed in (
    "WaveformExtractor::BuildCacheKey(",
    "state.database.upsertWaveformCacheStateByPath(",
    '"foreground_mini_rebuild_previous_failure"',
    '"foreground_waveform_rebuild_previous_failure"',
):
    assert removed not in pump, removed

assert CMAKE.count("src/WaveformRebuildFailurePersistence.cpp") == 1
assert BEHAVIOR.count("src/WaveformRebuildFailurePersistence.cpp") == 2
assert "TestWaveformRebuildFailurePersistence" in BEHAVIOR
assert "NAME TestWaveformRebuildFailurePersistenceBoundary" in BEHAVIOR

for forbidden in (
    "AppState.h",
    "AsyncWorker.h",
    "DiagnosticsLogger",
    "ModuleUI.h",
    "Platform::",
    "glDelete",
    "main.cpp",
    "WindowsMain",
):
    assert forbidden not in HEADER, forbidden
    assert forbidden not in SOURCE, forbidden

print("PASS: waveform rebuild failure persistence boundary contracts")

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER_PATH = ROOT / "src/WaveformRebuildRequestSubmission.h"
SOURCE_PATH = ROOT / "src/WaveformRebuildRequestSubmission.cpp"

assert HEADER_PATH.is_file(), f"missing header: {HEADER_PATH}"
assert SOURCE_PATH.is_file(), f"missing source: {SOURCE_PATH}"

HEADER = HEADER_PATH.read_text(encoding="utf-8")
SOURCE = SOURCE_PATH.read_text(encoding="utf-8")
WORKFLOW = (ROOT / "src/WaveformRebuildRecordWorkflow.cpp").read_text(
    encoding="utf-8"
)
SELECTED_WORKFLOW = (
    ROOT / "src/SelectedWaveformRebuildWorkflow.cpp"
).read_text(encoding="utf-8")
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
    "enum class WaveformRebuildSubmissionResult",
    "struct WaveformRebuildSubmissionRequest",
    "class WaveformRebuildSubmissionPort",
    "class WaveformWorkerRebuildSubmissionPort final",
    "WaveformRebuildSubmissionResult SubmitWaveformRebuildRequest(",
):
    assert token in HEADER, token

for owned in (
    "request.miniOnly",
    "port.submitMini(request.filePath)",
    "port.submitDetail(request.filePath, request.fileId)",
    "worker_.submitVisibleRequest(filePath)",
    "worker_.submitDetailRequest(filePath, fileId)",
):
    assert owned in SOURCE, owned

full_pump = function_body(MAIN, "static void PumpWaveformRebuild(AppState& state)")
selected_pump = function_body(
    MAIN, "static void PumpSelectedWaveformRebuild(AppState& state)"
)
assert MAIN.count("SubmitWaveformRebuildRequest(") == 0
assert WORKFLOW.count("SubmitWaveformRebuildRequest(") == 1
assert SELECTED_WORKFLOW.count("SubmitWaveformRebuildRequest(") == 1
for body in (full_pump, selected_pump):
    assert "WaveformWorkerRebuildSubmissionPort submissionPort(" in body
    assert "state.waveformWorker.submitVisibleRequest(" not in body
    assert "state.waveformWorker.submitDetailRequest(" not in body
assert "SubmitWaveformRebuildRequest(" not in full_pump
assert "ProcessNextWaveformRebuildRecord(" in full_pump
assert "SubmitWaveformRebuildRequest(" not in selected_pump

assert "++result.state.skipped;" in WORKFLOW
assert "effects.addPendingWaveform(item.filePath, !miniOnly);" in SELECTED_WORKFLOW

assert CMAKE.count("src/WaveformRebuildRequestSubmission.cpp") == 1
assert BEHAVIOR.count("src/WaveformRebuildRequestSubmission.cpp") == 3
assert "TestWaveformRebuildRequestSubmission" in BEHAVIOR
assert "NAME TestWaveformRebuildRequestSubmissionBoundary" in BEHAVIOR

for forbidden in (
    "AppState.h",
    "Database.h",
    "DiagnosticsLogger",
    "ModuleUI.h",
    "Platform::",
    "glDelete",
    "main.cpp",
    "WindowsMain",
):
    assert forbidden not in HEADER, forbidden
    assert forbidden not in SOURCE, forbidden
    assert forbidden not in SELECTED_WORKFLOW, forbidden

print("PASS: waveform rebuild request submission boundary contracts")

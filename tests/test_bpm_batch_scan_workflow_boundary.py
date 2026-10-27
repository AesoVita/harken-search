from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER_PATH = ROOT / "src/BpmBatchScanWorkflow.h"
SOURCE_PATH = ROOT / "src/BpmBatchScanWorkflow.cpp"

assert HEADER_PATH.is_file(), f"missing header: {HEADER_PATH}"
assert SOURCE_PATH.is_file(), f"missing source: {SOURCE_PATH}"

HEADER = HEADER_PATH.read_text(encoding="utf-8")
SOURCE = SOURCE_PATH.read_text(encoding="utf-8")
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
CMAKE = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
BEHAVIOR = (ROOT / "cmake/harken_search_behavior_tests.cmake").read_text(encoding="utf-8")


def function_body(source: str, signature: str) -> str:
    start = source.index(signature)
    brace = source.index("{", start + len(signature))
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
    "struct BpmBatchScanRow",
    "struct BpmBatchScanRequest",
    "class BpmBatchScanPort",
    "StartBpmBatchScan(",
):
    assert token in HEADER, token

for token in (
    "queryMissingBpm(50000, true, folderPathPrefix)",
    "port.cancelP2()",
    "port.setBatchTotal(static_cast<int>(rows.size()))",
    "port.setBatchDone(0)",
    "port.setBatchFound(0)",
    "port.setBatchRunning(true)",
    "port.setBatchCancelFlag(false)",
    "port.nextGeneration()",
    "port.submitP2(request)",
):
    assert token in SOURCE, token

for forbidden in (
    "AppState",
    "Database",
    "MusicAnalyzer",
    "MusicAnalysisRequest",
    "AnalysisPriority",
    "std::atomic",
    "DiagnosticsLogger",
    "ImGui",
    "Platform::",
    "main.cpp",
    "WindowsMain",
):
    assert forbidden not in HEADER, forbidden
    assert forbidden not in SOURCE, forbidden

root = function_body(
    MAIN,
    "static bool StartBpmBatchScan(AppState& state, "
    "const std::string& folderPathPrefix = {})",
)
assert root.count("aura::music_analysis::StartBpmBatchScan(") == 1
for moved in (
    "queryFilesMissingBpm",
    "cancelByPriority",
    "m_batchTotal",
    "m_batchDone",
    "m_batchFound",
    "m_batchRunning",
    "m_batchCancelFlag",
    "++state.bpmGeneration",
    "MusicAnalysisRequest req",
):
    assert moved not in root, moved

assert CMAKE.count("src/BpmBatchScanWorkflow.cpp") == 1
assert BEHAVIOR.count("src/BpmBatchScanWorkflow.cpp") == 1
assert "TestBpmBatchScanWorkflow" in BEHAVIOR
assert "NAME TestBpmBatchScanWorkflowBoundary" in BEHAVIOR

print("PASS: BPM batch scan workflow boundary contracts")

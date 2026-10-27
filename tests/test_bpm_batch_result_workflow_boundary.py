from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER_PATH = ROOT / "src/BpmBatchResultWorkflow.h"
SOURCE_PATH = ROOT / "src/BpmBatchResultWorkflow.cpp"

assert HEADER_PATH.is_file(), f"missing header: {HEADER_PATH}"
assert SOURCE_PATH.is_file(), f"missing source: {SOURCE_PATH}"

HEADER = HEADER_PATH.read_text(encoding="utf-8")
SOURCE = SOURCE_PATH.read_text(encoding="utf-8")
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
CMAKE = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
BEHAVIOR = (ROOT / "cmake/harken_search_behavior_tests.cmake").read_text(encoding="utf-8")

for token in (
    "struct BpmBatchResult",
    "struct BpmBatchViewUpdate",
    "class BpmBatchProgressPort",
    "class BpmBatchPersistencePort",
    "class BpmBatchViewPort",
    "ProcessBpmBatchResult(",
):
    assert token in HEADER, token

for forbidden in (
    "AppState",
    "Database",
    "MusicAnalyzer",
    "MusicAnalysisResult",
    "AnalysisPriority",
    "MusicFieldSource",
    "AudioRecord",
    "ImGui",
    "DiagnosticsLogger",
    "DIAG_",
    "Platform::",
    "platform/",
    "main.cpp",
    "WindowsMain",
):
    assert forbidden not in HEADER, forbidden
    assert forbidden not in SOURCE, forbidden

batch_prefix = MAIN.split("for (const auto& res : bpmResults) {", 1)[1].split(
    "ProcessSingleMusicAnalysisResult(", 1
)[0]
assert batch_prefix.count("ProcessBpmBatchResult(") == 1
for moved in (
    "res.priority == AnalysisPriority::P2",
    "m_batchDone.fetch_add",
    "setMusicBpm(",
    "m_batchFound.fetch_add",
    "setMusicKey(",
    "setMusicMood(",
    "snprintf(",
):
    assert moved not in batch_prefix, moved

assert "ProcessSingleMusicAnalysisResult(" in MAIN
assert CMAKE.count("src/BpmBatchResultWorkflow.cpp") == 1
assert BEHAVIOR.count("src/BpmBatchResultWorkflow.cpp") == 1
assert "TestBpmBatchResultWorkflow" in BEHAVIOR
assert "NAME TestBpmBatchResultWorkflowBoundary" in BEHAVIOR

print("PASS: BPM batch result workflow boundary contracts")

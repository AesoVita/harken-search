from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER_PATH = ROOT / "src/SingleMusicAnalysisResultWorkflow.h"
SOURCE_PATH = ROOT / "src/SingleMusicAnalysisResultWorkflow.cpp"

assert HEADER_PATH.is_file(), f"missing header: {HEADER_PATH}"
assert SOURCE_PATH.is_file(), f"missing source: {SOURCE_PATH}"

HEADER = HEADER_PATH.read_text(encoding="utf-8")
SOURCE = SOURCE_PATH.read_text(encoding="utf-8")
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
CMAKE = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
BEHAVIOR = (ROOT / "cmake/harken_search_behavior_tests.cmake").read_text(encoding="utf-8")

for token in (
    "struct SingleMusicAnalysisResult",
    "struct SingleMusicPendingUpdate",
    "class SingleMusicAnalysisStatePort",
    "class SingleMusicAnalysisPersistencePort",
    "class SingleMusicAnalysisViewPort",
    "ProcessSingleMusicAnalysisResult(",
):
    assert token in HEADER, token

for forbidden in (
    "AppState", "Database", "MusicAnalyzer",
    "AnalysisPriority", "MusicFieldSource", "AudioRecord", "ImGui",
    "DiagnosticsLogger", "DIAG_", "Platform::", "platform/", "main.cpp",
    "WindowsMain",
):
    assert forbidden not in HEADER, forbidden
    assert forbidden not in SOURCE, forbidden

ordinary = MAIN.split("ProcessBpmBatchResult(", 1)[1].split(
    "// Auto-dismiss bubble", 1
)[0]
assert ordinary.count("ProcessSingleMusicAnalysisResult(") == 1
for moved in (
    "res.generation != state.bpmGeneration",
    "pendingBpm.isAnalyzing = false",
    "setMusicKey(", "setMusicMood(", "pendingBpm.bpm",
    "beatGridOffset", "pendingBpm.failed",
):
    assert moved not in ordinary, moved

assert "pollResults(bpmResults, 64)" in MAIN
assert "pendingBpm.bubbleTimer += ImGui::GetIO().DeltaTime" in MAIN
assert "state.tapTempo.isExpired()" in MAIN
assert CMAKE.count("src/SingleMusicAnalysisResultWorkflow.cpp") == 1
assert BEHAVIOR.count("src/SingleMusicAnalysisResultWorkflow.cpp") == 1
assert "TestSingleMusicAnalysisResultWorkflow" in BEHAVIOR
assert "NAME TestSingleMusicAnalysisResultWorkflowBoundary" in BEHAVIOR

print("PASS: single music analysis result workflow boundary contracts")

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/BpmBatchCompletionWorkflow.h"
SOURCE = ROOT / "src/BpmBatchCompletionWorkflow.cpp"
assert HEADER.is_file(), f"missing header: {HEADER}"
assert SOURCE.is_file(), f"missing source: {SOURCE}"

header = HEADER.read_text(encoding="utf-8")
source = SOURCE.read_text(encoding="utf-8")
main = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
behavior = (ROOT / "cmake/harken_search_behavior_tests.cmake").read_text(encoding="utf-8")

for token in ("struct BpmBatchCompletionSnapshot", "class BpmBatchCompletionPort",
              "CompleteBpmBatchIfDrained("):
    assert token in header, token
for forbidden in ("AppState", "MusicAnalyzer", "atomic", "ImGui", "Database",
                  "Platform::", "platform/", "main.cpp", "WindowsMain"):
    assert forbidden not in header and forbidden not in source, forbidden

frame = main.split("// Mark T3 batch complete when queue drains", 1)[1].split(
    "// Deferred batch cover import", 1
)[0]
assert frame.count("CompleteBpmBatchIfDrained(") == 1
for moved in ("batchRunning()", "pendingCount() == 0", "isAnalyzing()",
              "m_batchRunning.store(false)", "browserMetaCacheDir.clear()"):
    assert moved not in frame, moved
assert cmake.count("src/BpmBatchCompletionWorkflow.cpp") == 1
assert behavior.count("src/BpmBatchCompletionWorkflow.cpp") == 1
assert "TestBpmBatchCompletionWorkflow" in behavior
print("PASS: BPM batch completion workflow boundary contracts")

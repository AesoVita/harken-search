from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER_PATH = ROOT / "src/SelectedRecordRescanWorkflow.h"
SOURCE_PATH = ROOT / "src/SelectedRecordRescanWorkflow.cpp"

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
    "struct SelectedRecordRescanItem",
    "struct SelectedRecordRescanMetadata",
    "struct SelectedRecordRescanFileFacts",
    "struct SelectedRecordRescanAudioFacts",
    "struct SelectedRecordRescanUpdate",
    "enum class SelectedRecordRescanItemResult",
    "struct SelectedRecordRescanBatchResult",
    "class SelectedRecordRescanOperationPort",
    "class SelectedRecordRescanBatchPort",
    "RescanSelectedRecord(",
    "RescanSelectedRecords(",
):
    assert token in HEADER, token

for forbidden in (
    "AppState",
    "Database.h",
    "MetadataReader",
    "DiagnosticsLogger",
    "ImGui",
    "Platform::",
    "main.cpp",
    "WindowsMain",
):
    assert forbidden not in HEADER, forbidden
    assert forbidden not in SOURCE, forbidden

batch = function_body(
    MAIN,
    "static void RescanSelectedFiles(AppState& state, const std::vector<AudioRecord>& records)",
)
assert batch.count("aura::record_rescan::RescanSelectedRecords(") == 1
assert "EXCEPTION_EXECUTE_HANDLER" in MAIN
assert '"Rescan.FileCrash"' in MAIN
assert "ImGui::GetTime() + 4.0" in batch
assert "RefreshSearchResults(state, false)" in batch

batch_port = MAIN.split("class AppSelectedRecordRescanBatchPort final", 1)[1].split(
    "private:", 1
)[0]
assert "rollbackTransaction()" in batch_port
assert "if (!committed)" in batch_port

for moved in (
    "MetadataReader::readMetadata",
    "ProbeRegularFile",
    "probeAudioFileInfo",
    "getWaveformCacheStateByPath",
    "clearWaveformCacheByPath",
    "updateFile(",
    "beginTransaction(",
    "commitTransaction(",
):
    assert moved not in batch, moved

assert "static bool RescanOneFileImpl" not in MAIN
assert CMAKE.count("src/SelectedRecordRescanWorkflow.cpp") == 1
assert BEHAVIOR.count("src/SelectedRecordRescanWorkflow.cpp") == 1
assert "TestSelectedRecordRescanWorkflow" in BEHAVIOR
assert "NAME TestSelectedRecordRescanWorkflowBoundary" in BEHAVIOR

print("PASS: selected record rescan workflow boundary contracts")

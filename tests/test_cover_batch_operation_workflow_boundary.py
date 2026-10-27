from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/CoverBatchOperationWorkflow.h"
SOURCE = ROOT / "src/CoverBatchOperationWorkflow.cpp"
assert HEADER.is_file(), f"missing header: {HEADER}"
assert SOURCE.is_file(), f"missing source: {SOURCE}"

header = HEADER.read_text(encoding="utf-8")
source = SOURCE.read_text(encoding="utf-8")
main = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
behavior = (ROOT / "cmake/harken_search_behavior_tests.cmake").read_text(encoding="utf-8")

for token in (
    "struct CoverBatchOperationInput",
    "class CoverBatchPersistencePort",
    "class CoverBatchViewPort",
    "class CoverBatchStatePort",
    "ResolveCoverImageMime(",
    "PumpCoverBatchOperation(",
):
    assert token in header, token

for forbidden in (
    "AppState", "Database", "AudioRecord", "CoverModule", "ImGui",
    "Localization", "Platform::", "platform/", "main.cpp", "WindowsMain",
):
    assert forbidden not in header and forbidden not in source, forbidden

pump = main.split("static void PumpCoverBatchOperation(AppState& state)", 1)[1].split(
    "// Explicit physical embedding", 1
)[0]
assert pump.count("aura::cover_batch::PumpCoverBatchOperation(") == 1
for moved in (
    "XXH3_64bits", "Hex64Local", "hasCoverThumb(", "saveCoverThumb(",
    "assignEmbeddedCoverByPaths(", "clearCoverByPaths(", "searchResults",
    "InvalidateFileCoverCaches(", "ClearFolderCoverNegativeCache(",
    "RequestCoverWallDatabaseInvalidation(", "coverBatchProgress =",
    "coverBatchActive = false", "coverBatchTargets.clear()",
):
    assert moved not in pump, moved
assert "static std::string GuessImageMime(" not in main
assert main.count("aura::cover_batch::ResolveCoverImageMime(") == 2
assert cmake.count("src/CoverBatchOperationWorkflow.cpp") == 1
assert behavior.count("src/CoverBatchOperationWorkflow.cpp") == 1
assert "TestCoverBatchOperationWorkflow" in behavior
assert "NAME TestCoverBatchOperationWorkflowBoundary" in behavior
print("PASS: cover batch operation workflow boundary contracts")

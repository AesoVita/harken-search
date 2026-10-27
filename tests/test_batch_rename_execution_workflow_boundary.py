from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER_PATH = ROOT / "src/BatchRenameExecutionWorkflow.h"
SOURCE_PATH = ROOT / "src/BatchRenameExecutionWorkflow.cpp"

assert HEADER_PATH.is_file()
assert SOURCE_PATH.is_file()

HEADER = HEADER_PATH.read_text(encoding="utf-8")
SOURCE = SOURCE_PATH.read_text(encoding="utf-8")
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
CMAKE = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
BEHAVIOR = (ROOT / "cmake/harken_search_behavior_tests.cmake").read_text(encoding="utf-8")
RULES = (ROOT / "docs/architecture/stage-4-dependency-rules.json").read_text(
    encoding="utf-8"
)
STAGE6 = (ROOT / "tests/test_stage6_platform_boundary.py").read_text(
    encoding="utf-8"
)
STAGE7 = (ROOT / "tools/stage7_api_contract_audit.py").read_text(
    encoding="utf-8"
)

for required in (
    "struct BatchRenameExecutionLabels",
    "struct CompletedBatchRename",
    "using BatchRenameOperation = std::function<bool(",
    "std::vector<CompletedBatchRename> ExecuteEligibleRenames(",
):
    assert required in HEADER

for forbidden in (
    "AppState",
    "UndoSystem",
    "Database",
    "AudioEngine",
    "Localization",
    "imgui",
    "WindowsMain",
    "main.cpp",
    "platform/",
    "windows.h",
):
    assert forbidden not in HEADER
    assert forbidden not in SOURCE

assert '#include "BatchRenamePolicy.h"' in HEADER
assert '#include "Utils.h"' in SOURCE
assert "std::filesystem::rename" not in SOURCE
assert "Platform::" not in SOURCE
assert "renameOperation(" in SOURCE
assert "catch (const std::exception&)" in SOURCE
assert "catch (...)" in SOURCE

wrapper = MAIN.split("static void ExecuteBatchRename(AppState& state)", 1)[1].split(
    "// Helper: check if folder name matches search", 1
)[0]
assert wrapper.count("aura::rename::ExecuteEligibleRenames(") == 1
assert "RenameFileOnDisk(state, recordId, oldPath, newName)" in wrapper
assert "std::make_unique<FileRenameOp>()" in wrapper
assert "state.undoStack.push(std::move(undoOp))" in wrapper
assert "state.setFolderHierarchy(" in wrapper
assert "RefreshSearchResults(state, true)" in wrapper
assert "state.clearSelection()" in wrapper
for forbidden in (
    "for (auto& preview : state.renamePreviewList)",
    "catch (const std::exception&)",
    "catch (...)",
):
    assert forbidden not in wrapper

assert CMAKE.count("src/BatchRenameExecutionWorkflow.cpp") == 1
assert "NAME TestBatchRenameExecutionWorkflow" in BEHAVIOR
assert "NAME TestBatchRenameExecutionWorkflowBoundary" in BEHAVIOR
assert "tests/test_batch_rename_execution_workflow_boundary.py" in BEHAVIOR
assert '"src/BatchRenameExecutionWorkflow.cpp"' in RULES
assert '"src/BatchRenameExecutionWorkflow.h"' in RULES
assert "test_batch_rename_execution_workflow_owns_status_state_machine" in STAGE6
assert (
    '"src/BatchRenameExecutionWorkflow.h": '
    '"tests/test_batch_rename_execution_workflow_boundary.py"'
    in STAGE7
)

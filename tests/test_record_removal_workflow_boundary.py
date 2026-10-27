from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER_PATH = ROOT / "src/RecordRemovalWorkflow.h"
SOURCE_PATH = ROOT / "src/RecordRemovalWorkflow.cpp"

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
STAGE7 = (ROOT / "tools/stage7_api_contract_audit.py").read_text(encoding="utf-8")

for required in (
    "class Database;",
    "class AudioEngine;",
    "struct FileDeletionOutcome",
    "std::vector<std::string> filesystemFailures;",
    "void RemoveRecordsFromDatabase(",
    "FileDeletionOutcome DeleteFilesFromSystem(",
):
    assert required in HEADER
for forbidden in (
    "AppState",
    "imgui",
    "windows.h",
    "WindowsMain",
    "main.cpp",
    "platform/filesystem_",
):
    assert forbidden not in HEADER
    assert forbidden not in SOURCE

for required_include in (
    '#include "Database.h"',
    '#include "AudioEngine.h"',
    '#include "platform/filesystem.h"',
):
    assert required_include in SOURCE

remove_start = SOURCE.index("void RemoveRecordsFromDatabase(")
delete_start = SOURCE.index("FileDeletionOutcome DeleteFilesFromSystem(")
remove_body = SOURCE[remove_start:delete_start]
delete_body = SOURCE[delete_start:]

busy = remove_body.index('executeRawQuery("PRAGMA busy_timeout = 5000;")')
begin = remove_body.index("beginTransaction()")
remove = remove_body.index("removeFile(id)")
commit = remove_body.index("commitTransaction()")
facet = remove_body.index("invalidateFacetQueryCache()")
folder = remove_body.index("clearFolderIndex()")
assert busy < begin < remove < commit < facet < folder

loaded = delete_body.index("getLoadedFilePath()")
unload = delete_body.index("audioEngine.unload()")
busy = delete_body.index('executeRawQuery("PRAGMA busy_timeout = 5000;")')
begin = delete_body.index("beginTransaction()")
probe = delete_body.index("Platform::FileSystem::ProbePath")
physical = delete_body.index("Platform::FileSystem::RemoveFile")
failure = delete_body.index("outcome.filesystemFailures.push_back")
remove = delete_body.index("database.removeFile(ids[i])")
commit = delete_body.index("commitTransaction()")
facet = delete_body.index("invalidateFacetQueryCache()")
folder = delete_body.index("clearFolderIndex()")
assert loaded < unload < busy < begin < probe < physical < failure < remove
assert remove < commit < facet < folder

qualified_remove = "aura::records::RemoveRecordsFromDatabase(state.database, ids)"
qualified_delete = (
    "aura::records::DeleteFilesFromSystem(\n"
    "        state.database, state.audioEngine, ids, paths)"
)
assert MAIN.count(qualified_remove) == 1
assert MAIN.count(qualified_delete) == 1

main_remove_start = MAIN.index("static void RemoveRecordsFromDatabase(")
main_delete_start = MAIN.index("static void DeleteFilesFromSystem(")
main_rename_start = MAIN.index("// Rename file on disk", main_delete_start)
main_remove = MAIN[main_remove_start:main_delete_start]
main_delete = MAIN[main_delete_start:main_rename_start]
for body in (main_remove, main_delete):
    assert "executeRawQuery" not in body
    assert "beginTransaction" not in body
    assert "commitTransaction" not in body
    assert "invalidateFacetQueryCache" not in body
    assert "clearFolderIndex" not in body
assert "Platform::FileSystem::ProbePath" not in main_delete
assert "Platform::FileSystem::RemoveFile" not in main_delete

call = main_remove.index(qualified_remove)
erase = main_remove.index("state.searchResults.erase", call)
cover = main_remove.index(
    "aura::cover_wall::RequestDatabaseInvalidation", erase
)
stop = main_remove.index("state.audioEngine.stop()", cover)
hierarchy = main_remove.index("state.setFolderHierarchy", stop)
refresh = main_remove.index("RefreshSearchResults(state, true)", hierarchy)
assert call < erase < cover < stop < hierarchy < refresh

call = main_delete.index("aura::records::DeleteFilesFromSystem(")
toast = main_delete.index("if (!outcome.filesystemFailures.empty())", call)
erase = main_delete.index("state.searchResults.erase", toast)
cover = main_delete.index(
    "aura::cover_wall::RequestDatabaseInvalidation", erase
)
current = main_delete.index("state.currentFileId = -1", cover)
hierarchy = main_delete.index("state.setFolderHierarchy", current)
refresh = main_delete.index("RefreshSearchResults(state, false)", hierarchy)
assert call < toast < erase < cover < current < hierarchy < refresh

assert CMAKE.count("src/RecordRemovalWorkflow.cpp") == 1
assert "NAME TestRecordRemovalWorkflowBoundary" in BEHAVIOR
assert "tests/test_record_removal_workflow_boundary.py" in BEHAVIOR
assert '"src/RecordRemovalWorkflow.cpp"' in RULES
assert '"src/RecordRemovalWorkflow.h"' in RULES
assert (
    '"src/RecordRemovalWorkflow.h": '
    '"tests/test_record_removal_workflow_boundary.py"'
    in STAGE7
)

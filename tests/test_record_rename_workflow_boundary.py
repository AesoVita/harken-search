from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER_PATH = ROOT / "src/RecordRenameWorkflow.h"
SOURCE_PATH = ROOT / "src/RecordRenameWorkflow.cpp"

assert HEADER_PATH.is_file()
assert SOURCE_PATH.is_file()

HEADER = HEADER_PATH.read_text(encoding="utf-8")
SOURCE = SOURCE_PATH.read_text(encoding="utf-8")
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
MODULE_UI = (ROOT / "src/ModuleUI.cpp").read_text(encoding="utf-8")
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
    "class Database;",
    "class AudioEngine;",
    "struct RecordRenameOutcome",
    "bool success = false;",
    "bool reloadRenamedAudio = false;",
    "std::string newPath;",
    "RecordRenameOutcome RenameFileAndDatabase(",
):
    assert required in HEADER
for forbidden in (
    "AppState",
    "WaveformData",
    "AudioRecord",
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
    '#include "Utils.h"',
    '#include "platform/filesystem.h"',
):
    assert required_include in SOURCE

body = SOURCE[SOURCE.index("RecordRenameOutcome RenameFileAndDatabase(") :]
source_exists = body.index("Platform::FileSystem::Exists(PathToUtf8(oldP))")
target_path = body.index("oldP.parent_path() / Utf8ToPath(newFileName)")
target_exists = body.index("Platform::FileSystem::Exists(PathToUtf8(newP))")
new_path = body.index("outcome.newPath = PathToUtf8(newP)")
forward_note = body.index("database.noteExpectedFileRename(oldPath, outcome.newPath)")
first_rename = body.index(
    "Platform::FileSystem::Rename(oldPath, outcome.newPath, &renameError)"
)
loaded = body.index("audioEngine.isLoaded()", first_rename)
exact_path = body.index("audioEngine.getFileInfo().filePath == oldPath", loaded)
stop = body.index("audioEngine.stop()", exact_path)
unload = body.index("audioEngine.unload()", stop)
second_rename = body.index(
    "Platform::FileSystem::Rename(oldPath, outcome.newPath, &renameError)",
    first_rename + 1,
)
db_update = body.index(
    "database.updateFilePath(oldPath, outcome.newPath, newFileName)", second_rename
)
reverse_note = body.index(
    "database.noteExpectedFileRename(outcome.newPath, oldPath)", db_update
)
rollback = body.index(
    "Platform::FileSystem::Rename(outcome.newPath, oldPath, &rollbackError)",
    reverse_note,
)
db_failure_reload = body.index("audioEngine.loadFileAsync(oldPath)", rollback)
success = body.index("outcome.success = true", db_failure_reload)
reload_flag = body.index("outcome.reloadRenamedAudio = wasPlayingThis", success)
catch = body.index("} catch (...) {", reload_flag)
catch_unloaded = body.index("if (!audioEngine.isLoaded())", catch)
catch_reload = body.index("audioEngine.loadFileAsync(oldPath)", catch_unloaded)
assert source_exists < target_path < target_exists < new_path < forward_note
assert forward_note < first_rename < loaded < exact_path < stop < unload
assert unload < second_rename < db_update < reverse_note < rollback
assert rollback < db_failure_reload < success < reload_flag < catch
assert catch < catch_unloaded < catch_reload

public_signature = (
    "bool RenameFileOnDisk(AppState& state,\n"
    "                      int64_t recordId,\n"
    "                      const std::string& oldPath,\n"
    "                      const std::string& newFileName)"
)
assert MAIN.count(public_signature) == 1
assert MODULE_UI.count("bool RenameFileOnDisk(AppState& state,") == 1
assert MAIN.count(
    "RenameFileOnDisk(state, recordId, oldPath, newName)"
) == 1
assert MODULE_UI.count(
    "RenameFileOnDisk(state, rec.id, rec.filePath, newFileName)"
) == 1

rename_start = MAIN.index(public_signature)
rename_end = MAIN.index("// Batch Rename Helper Functions", rename_start)
wrapper = MAIN[rename_start:rename_end]
qualified_call = (
    "aura::records::RenameFileAndDatabase(\n"
    "        state.database, state.audioEngine, oldPath, newFileName)"
)
assert wrapper.count(qualified_call) == 1
for forbidden in (
    "Platform::FileSystem::Exists",
    "Platform::FileSystem::Rename",
    "noteExpectedFileRename",
    "updateFilePath",
    "audioEngine.stop()",
    "audioEngine.unload()",
):
    assert forbidden not in wrapper

call = wrapper.index(qualified_call)
failure = wrapper.index("if (!outcome.success) return false", call)
compose_try = wrapper.index("try {", failure)
cache = wrapper.index("state.waveformCache.find(oldPath)", compose_try)
row = wrapper.index("for (auto& r : state.searchResults)", cache)
reload_guard = wrapper.index("if (outcome.reloadRenamedAudio)", row)
renamed_reload = wrapper.index(
    "state.audioEngine.loadFileAsync(outcome.newPath)", reload_guard
)
return_true = wrapper.index("return true", renamed_reload)
compose_catch = wrapper.index("} catch (...) {", return_true)
compose_unloaded = wrapper.index("if (!state.audioEngine.isLoaded())", compose_catch)
old_reload = wrapper.index("state.audioEngine.loadFileAsync(oldPath)", compose_unloaded)
return_false = wrapper.index("return false", old_reload)
assert call < failure < compose_try < cache < row < reload_guard
assert reload_guard < renamed_reload < return_true < compose_catch
assert compose_catch < compose_unloaded < old_reload < return_false
assert "outcome.newPath" in wrapper
assert "fs::path newP" not in wrapper

assert CMAKE.count("src/RecordRenameWorkflow.cpp") == 1
assert "NAME TestRecordRenameWorkflowBoundary" in BEHAVIOR
assert "tests/test_record_rename_workflow_boundary.py" in BEHAVIOR
assert '"src/RecordRenameWorkflow.cpp"' in RULES
assert '"src/RecordRenameWorkflow.h"' in RULES
assert "test_record_rename_workflow_owns_transaction_side_effects" in STAGE6
assert (
    '"src/RecordRenameWorkflow.h": '
    '"tests/test_record_rename_workflow_boundary.py"'
    in STAGE7
)

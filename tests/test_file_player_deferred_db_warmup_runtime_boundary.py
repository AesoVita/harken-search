from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER_PATH = ROOT / "src/FilePlayerDeferredDbWarmupRuntime.h"
RUNTIME_PATH = ROOT / "src/FilePlayerDeferredDbWarmupRuntime.cpp"
OPERATION_PATH = ROOT / "src/FilePlayerDeferredDbWarmupOperation.cpp"

assert HEADER_PATH.is_file(), f"missing header: {HEADER_PATH}"
assert RUNTIME_PATH.is_file(), f"missing runtime source: {RUNTIME_PATH}"
assert OPERATION_PATH.is_file(), f"missing operation source: {OPERATION_PATH}"

HEADER = HEADER_PATH.read_text(encoding="utf-8")
RUNTIME = RUNTIME_PATH.read_text(encoding="utf-8")
OPERATION = OPERATION_PATH.read_text(encoding="utf-8")
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
    "enum class DeferredDatabaseWarmupStartResult",
    "class DeferredDatabaseWarmupRuntime",
    "explicit DeferredDatabaseWarmupRuntime(",
    "DeferredDatabaseWarmupStartResult Start(std::string dbPathUtf8);",
    "bool PollFinished(bool& success);",
    "bool WarmFilePlayerDatabaseReadOnly(",
):
    assert required in HEADER

assert '#include "platform/filesystem.h"' in RUNTIME
assert "Platform::FileSystem::Exists(dbPathUtf8)" in RUNTIME
assert "catch (...)" in RUNTIME
assert "std::thread([task, operation" in RUNTIME
assert "}).detach();" in RUNTIME
assert "std::memory_order_release" in RUNTIME
assert "std::memory_order_acquire" in RUNTIME
assert "std::memory_order_acq_rel" in RUNTIME

assert "Platform::SetThreadBackgroundPriority()" in OPERATION
operation_positions = [
    OPERATION.index(token)
    for token in ("openReadOnly", "getFileCount", "warmupCoverIndexes", "close")
]
assert operation_positions == sorted(operation_positions)

for forbidden in (
    "main.cpp",
    "WindowsMain",
    "AppState",
    "imgui",
    "platform/system_win32",
):
    assert forbidden not in HEADER
    assert forbidden not in RUNTIME
    assert forbidden not in OPERATION

for removed in (
    "s_filePlayerDeferredDbWarmupStarted",
    "s_filePlayerDeferredDbWarmupDone",
    "s_filePlayerDeferredDbWarmupSuccess",
    "s_filePlayerDeferredDbWarmupReported",
    "Database warmupDb",
    "warmupDb.openReadOnly",
):
    assert removed not in MAIN

for retained in (
    "!state.filePlayerFastPathActive || state.database.isOpen()",
    "ResolveFilePlayerDatabasePath(state)",
    "Deferred database warmup skipped: path unavailable path=%s",
    "Deferred database warmup started: path=%s",
    "Deferred database warmup finished: success=%d",
):
    assert retained in MAIN

assert '#include "FilePlayerDeferredDbWarmupRuntime.h"' in MAIN
assert MAIN.count("DeferredDatabaseWarmupRuntime") == 1
assert "WarmFilePlayerDatabaseReadOnly" in MAIN
assert ".Start(dbPathUtf8)" in MAIN
assert ".PollFinished(success)" in MAIN

assert CMAKE.count("src/FilePlayerDeferredDbWarmupRuntime.cpp") == 1
assert CMAKE.count("src/FilePlayerDeferredDbWarmupOperation.cpp") == 1
assert "NAME TestFilePlayerDeferredDbWarmupRuntime" in BEHAVIOR
assert "NAME TestFilePlayerDeferredDbWarmupRuntimeBoundary" in BEHAVIOR
for governed in (
    '"src/FilePlayerDeferredDbWarmupRuntime.h"',
    '"src/FilePlayerDeferredDbWarmupRuntime.cpp"',
    '"src/FilePlayerDeferredDbWarmupOperation.cpp"',
    '"src/platform/system.h"',
):
    assert governed in RULES
assert "test_file_player_deferred_db_warmup_runtime_owns_async_session" in STAGE6
assert (
    '"src/FilePlayerDeferredDbWarmupRuntime.h": '
    '"tests/test_file_player_deferred_db_warmup_runtime_boundary.py"'
    in STAGE7
)

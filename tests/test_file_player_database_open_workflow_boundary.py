from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER_PATH = ROOT / "src/FilePlayerDatabaseOpenWorkflow.h"
CORE_PATH = ROOT / "src/FilePlayerDatabaseOpenWorkflow.cpp"
ADAPTER_PATH = ROOT / "src/FilePlayerDatabaseOpenAdapter.cpp"

assert HEADER_PATH.is_file(), f"missing header: {HEADER_PATH}"
assert CORE_PATH.is_file(), f"missing core source: {CORE_PATH}"
assert ADAPTER_PATH.is_file(), f"missing adapter source: {ADAPTER_PATH}"

HEADER = HEADER_PATH.read_text(encoding="utf-8")
CORE = CORE_PATH.read_text(encoding="utf-8")
ADAPTER = ADAPTER_PATH.read_text(encoding="utf-8")
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
    "struct DatabaseOpenAttemptResult",
    "using DatabaseOpenAttempt =",
    "struct FilePlayerDatabaseOpenResult",
    "bool fallbackAttempted = false;",
    "std::string activePathUtf8;",
    "FilePlayerDatabaseOpenResult OpenFilePlayerDatabaseWithFallback(",
    "Database& database",
):
    assert required in HEADER

assert '#include "Database.h"' not in HEADER
assert '#include "Database.h"' not in CORE
assert '#include "Database.h"' in ADAPTER
assert "database.open(pathUtf8, true)" in ADAPTER
assert "Platform::HarkenUserData::EnsureLayout().readOnly" in ADAPTER
assert "database.openReadOnly(pathUtf8)" in ADAPTER
assert "database.getLastError()" in ADAPTER
assert "fallbackPathUtf8 == result.activePathUtf8" in CORE
assert "result.fallbackAttempted = true" in CORE

for forbidden in (
    "main.cpp",
    "WindowsMain",
    "AppState",
    "ImGui",
    "Localization",
    "platform/system_win32",
):
    assert forbidden not in HEADER + CORE + ADAPTER

mount_body = MAIN.split(
    "static bool MountFilePlayerDatabaseForInteractiveRefresh", 1
)[1].split("void RefreshSearchResults", 1)[0]
assert "state.database.open(" not in mount_body
assert "state.database.getLastError()" not in mount_body
assert "OpenFilePlayerDatabaseWithFallback(" in mount_body
assert "openResult.activePathUtf8" in mount_body
assert "openResult.error" in mount_body
for retained in (
    "Interactive database mount started: fullPreheat=%d path=%s",
    "Interactive database mount failed: path=%s error=%s",
    "Interactive database mount complete: path=%s",
    "state.loadErrorToast",
    "ImGui::GetTime() + 5.0",
    "state.filterQueryState = FilterQueryState::Error",
    "ConfigureDatabaseRuntimeForFilePlayerMount(state, dbPathUtf8)",
    "SyncAdaptiveMiniWaveformFormatUpgradeStateOnOpen(state)",
    "state.database.preheat()",
    "state.database.enqueueStartupReconciliation()",
    "state.searchWorker.start(&state.database)",
    "state.waveformWorker.start(&state.database",
    "StartDeferredSidebarBootstrap(state, true, true)",
):
    assert retained in mount_body

assert '#include "FilePlayerDatabaseOpenWorkflow.h"' in MAIN
assert CMAKE.count("src/FilePlayerDatabaseOpenWorkflow.cpp") == 1
assert CMAKE.count("src/FilePlayerDatabaseOpenAdapter.cpp") == 1
target = BEHAVIOR.split(
    "add_executable(TestFilePlayerDatabaseOpenWorkflow EXCLUDE_FROM_ALL", 1
)[1].split("add_test(", 1)[0]
assert "src/FilePlayerDatabaseOpenWorkflow.cpp" in target
assert "FilePlayerDatabaseOpenAdapter.cpp" not in target
assert "Database.cpp" not in target
assert "NAME TestFilePlayerDatabaseOpenWorkflow" in BEHAVIOR
assert "NAME TestFilePlayerDatabaseOpenWorkflowBoundary" in BEHAVIOR
for governed in (
    '"src/FilePlayerDatabaseOpenWorkflow.h"',
    '"src/FilePlayerDatabaseOpenWorkflow.cpp"',
    '"src/FilePlayerDatabaseOpenAdapter.cpp"',
):
    assert governed in RULES
assert "test_file_player_database_open_workflow_owns_fallback_policy" in STAGE6
assert (
    '"src/FilePlayerDatabaseOpenWorkflow.h": '
    '"tests/test_file_player_database_open_workflow_boundary.py"'
    in STAGE7
)

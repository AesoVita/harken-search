from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src" / "DatabaseSwitchBlockingOperation.h"
SOURCE = ROOT / "src" / "DatabaseSwitchBlockingOperation.cpp"
ADAPTER = ROOT / "src" / "DatabaseSwitchBlockingOperationAdapter.cpp"
MAIN = ROOT / "src" / "main.cpp"


def require(text: str, needle: str, source: str) -> None:
    assert needle in text, f"missing {needle!r} in {source}"


def reject(text: str, needle: str, source: str) -> None:
    assert needle not in text, f"forbidden {needle!r} remains in {source}"


def main() -> None:
    assert HEADER.exists(), f"missing production header: {HEADER}"
    assert SOURCE.exists(), f"missing production source: {SOURCE}"
    assert ADAPTER.exists(), f"missing production adapter: {ADAPTER}"

    header = HEADER.read_text(encoding="utf-8")
    source = SOURCE.read_text(encoding="utf-8")
    adapter = ADAPTER.read_text(encoding="utf-8")
    root = MAIN.read_text(encoding="utf-8")
    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    behavior = (ROOT / "cmake" / "harken_search_behavior_tests.cmake").read_text(
        encoding="utf-8"
    )
    rules = (ROOT / "docs" / "architecture" / "stage-4-dependency-rules.json").read_text(
        encoding="utf-8"
    )
    stage6 = (ROOT / "tests" / "test_stage6_platform_boundary.py").read_text(
        encoding="utf-8"
    )
    stage7 = (ROOT / "tools" / "stage7_api_contract_audit.py").read_text(
        encoding="utf-8"
    )

    for needle in (
        "struct BlockingDatabaseSwitchRequest",
        "const std::string& oldDbPathUtf8;",
        "const std::string& newDbPathUtf8;",
        "int initialWaveformCacheMode = -1;",
        "bool autoReconcileOnStartup = false;",
        "class BlockingDatabaseSwitchEnvironment",
        "virtual void JoinBatchFeatures() = 0;",
        "virtual void StopSameFolderScan() = 0;",
        "virtual void StopSearch() = 0;",
        "virtual void StopWaveform() = 0;",
        "virtual void StopBackfill() = 0;",
        "virtual void StopCovers() = 0;",
        "virtual void CloseDatabase() = 0;",
        "virtual bool OpenDatabase(const std::string& pathUtf8) = 0;",
        "virtual bool SetWaveformCacheMode(int mode) = 0;",
        "virtual int EnqueueStartupReconciliation() = 0;",
        "RunBlockingDatabaseSwitchOperation(",
    ):
        require(header, needle, HEADER.name)

    ordered = (
        "stage=stop_batch_features",
        "environment.JoinBatchFeatures();",
        "stage=stop_same_folder",
        "environment.StopSameFolderScan();",
        "stage=stop_search",
        "environment.StopSearch();",
        "stage=stop_waveform",
        "environment.StopWaveform();",
        "stage=stop_backfill",
        "environment.StopBackfill();",
        "environment.StopCovers();",
        "stage=close_old_db",
        "environment.CloseDatabase();",
        "stage=open_new_db",
        "environment.OpenDatabase(request.newDbPathUtf8)",
        "environment.SetWaveformCacheMode(",
        "environment.EnqueueStartupReconciliation()",
        "stage=done",
        "return ok;",
    )
    positions = [source.index(token) for token in ordered]
    assert positions == sorted(positions), "blocking operation order changed"

    for needle in (
        "class ConcreteBlockingDatabaseSwitchEnvironment final",
        "batchFeatureThread_.joinable()",
        "batchFeatureThread_.join();",
        "sameFolderScanner_.stop();",
        "searchWorker_.stop();",
        "waveformWorker_.stop();",
        "backfillWorkerPool_.stop();",
        "CoverModule::ShutdownCoverWallPrecache();",
        "database_.close();",
        "database_.open(pathUtf8, true)",
        "database_.setWaveformCacheMode(",
        "database_.enqueueStartupReconciliation()",
    ):
        require(adapter, needle, ADAPTER.name)

    for needle in (
        "SwitchDatabaseInitialWaveformPolicy",
        "SwitchDatabase AutoReconcile: queued %d watched roots",
        "SwitchDatabase AutoReconcile skipped: setting is opt-in and currently disabled",
    ):
        require(source, needle, SOURCE.name)

    recovery = root.split("if (!oldPath.empty() && state.database.isOpen()) {", 1)[1].split("state.searchWorker.start", 1)[0]
    recovery_steps = ("state.currentDbPathUtf8 = oldPath;",
                      "PushRecentDbPath(state, oldPath);", "SaveAppSettings(state);")
    for step in recovery_steps:
        require(recovery, step, "recovered database must survive restart")
    positions = [recovery.index(step) for step in recovery_steps]
    assert positions == sorted(positions), "save the actual recovered path and recent entry"

    covers = (ROOT / "src" / "CoverModule.cpp").read_text(encoding="utf-8")
    async_start = covers.split("static bool StartAsyncLoader()", 1)[1].split(
        "// -------------------------", 1
    )[0]
    wall_start = covers.split("static bool EnsureJobThreadStarted()", 1)[1].split(
        "static bool SeenKey", 1
    )[0]
    for body in (async_start, wall_start):
        require(body, "dbSwitchInProgress) return false", "cover worker admission")
        reject(body, "call_once", "restartable cover worker admission")
    require(async_start, "g_asyncThreads.empty()", "decoder restart")
    require(wall_start, "g_jobExit.store(false)", "cover wall restart")

    for forbidden in (
        "AppState",
        "WindowsMain",
        "ImGui",
        "glDelete",
        "Localization",
        "filesystem_win32",
        "system_win32",
    ):
        reject(header + source + adapter, forbidden, "operation files")

    worker_region = root.split("g_databaseSwitchWorkerRuntime.Start(", 1)[1].split(
        "return true;", 1
    )[0]
    require(root, '#include "DatabaseSwitchBlockingOperation.h"', MAIN.name)
    require(worker_region, "RunBlockingDatabaseSwitchOperation(", "root worker region")
    for old_owner in (
        "batchFeatureThread.join()",
        "g_sameFolderScanner.stop()",
        "searchWorker.stop()",
        "waveformWorker.stop()",
        "GlobalBackfillWorkerPool().stop()",
        "database.close()",
        "database.open(",
        "database.setWaveformCacheMode(",
        "database.enqueueStartupReconciliation()",
        "SwitchDatabaseWorkerStage:",
    ):
        reject(worker_region, old_owner, "root worker region")

    require(cmake, "src/DatabaseSwitchBlockingOperation.cpp", "CMakeLists.txt")
    require(cmake, "src/DatabaseSwitchBlockingOperationAdapter.cpp", "CMakeLists.txt")
    require(behavior, "TestDatabaseSwitchBlockingOperation", "behavior CMake")
    require(behavior, "TestDatabaseSwitchBlockingOperationBoundary", "behavior CMake")
    require(rules, '"src/DatabaseSwitchBlockingOperation.cpp"', "Stage 4 rules")
    require(stage6, "test_database_switch_blocking_operation_owns_order", "Stage 6")
    require(stage7, '"src/DatabaseSwitchBlockingOperation.h"', "Stage 7 mapping")

    print("PASS: database switch blocking operation boundary contracts")


if __name__ == "__main__":
    main()

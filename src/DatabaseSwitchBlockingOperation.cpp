#include "DatabaseSwitchBlockingOperation.h"

#include "DiagnosticsLogger.h"

namespace aura::database_switch {

bool RunBlockingDatabaseSwitchOperation(
    BlockingDatabaseSwitchEnvironment& environment,
    const BlockingDatabaseSwitchRequest& request) {
    DIAG_SYSTEM("Database", "SwitchDatabaseWorkerStage: stage=stop_batch_features");
    environment.JoinBatchFeatures();
    DIAG_SYSTEM_FMT(
        "Database",
        "SwitchDatabaseWorkerStage: stage=stop_same_folder from=%s to=%s",
        request.oldDbPathUtf8.c_str(),
        request.newDbPathUtf8.c_str());
    environment.StopSameFolderScan();
    DIAG_SYSTEM("Database", "SwitchDatabaseWorkerStage: stage=stop_search");
    environment.StopSearch();
    DIAG_SYSTEM("Database", "SwitchDatabaseWorkerStage: stage=stop_waveform");
    environment.StopWaveform();
    DIAG_SYSTEM("Database", "SwitchDatabaseWorkerStage: stage=stop_backfill");
    environment.StopBackfill();
    DIAG_SYSTEM("Database", "SwitchDatabaseWorkerStage: stage=stop_covers");
    environment.StopCovers();
    DIAG_SYSTEM("Database", "SwitchDatabaseWorkerStage: stage=close_old_db");
    environment.CloseDatabase();
    DIAG_SYSTEM_FMT(
        "Database",
        "SwitchDatabaseWorkerStage: stage=open_new_db path=%s",
        request.newDbPathUtf8.c_str());
    bool ok = environment.OpenDatabase(request.newDbPathUtf8);
    if (ok && request.initialWaveformCacheMode >= 0) {
        const bool modeOk = environment.SetWaveformCacheMode(
            request.initialWaveformCacheMode);
        DIAG_SYSTEM_FMT(
            "Database",
            "SwitchDatabaseInitialWaveformPolicy: ok=%d mode=%d path=%s",
            modeOk ? 1 : 0,
            request.initialWaveformCacheMode,
            request.newDbPathUtf8.c_str());
        ok = modeOk;
    }
    if (ok && request.autoReconcileOnStartup) {
        const int count = environment.EnqueueStartupReconciliation();
        DIAG_SYSTEM_FMT(
            "Database",
            "SwitchDatabase AutoReconcile: queued %d watched roots",
            count);
    } else if (ok) {
        DIAG_SYSTEM(
            "Database",
            "SwitchDatabase AutoReconcile skipped: setting is opt-in and currently disabled");
    }
    DIAG_SYSTEM_FMT(
        "Database",
        "SwitchDatabaseWorkerStage: stage=done ok=%d path=%s",
        ok ? 1 : 0,
        request.newDbPathUtf8.c_str());
    return ok;
}

} // namespace aura::database_switch

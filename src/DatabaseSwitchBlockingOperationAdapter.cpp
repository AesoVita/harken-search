#include "DatabaseSwitchBlockingOperation.h"

#include "AsyncWorker.h"
#include "BackfillWorkerPool.h"
#include "CoverModule.h"
#include "Database.h"
#include "DatabasePackageReplacement.h"
#include "DiagnosticsLogger.h"
#include "SameFolderScanRuntime.h"

namespace aura::database_switch {
namespace {

class ConcreteBlockingDatabaseSwitchEnvironment final
    : public BlockingDatabaseSwitchEnvironment {
public:
    ConcreteBlockingDatabaseSwitchEnvironment(
        std::thread& batchFeatureThread,
        aura::same_folder::SameFolderScanRuntime& sameFolderScanner,
        SearchWorker& searchWorker,
        WaveformWorker& waveformWorker,
        aura::BackfillWorkerPool& backfillWorkerPool,
        Database& database,
        const BlockingDatabaseSwitchRequest& request)
        : batchFeatureThread_(batchFeatureThread),
          sameFolderScanner_(sameFolderScanner),
          searchWorker_(searchWorker),
          waveformWorker_(waveformWorker),
          backfillWorkerPool_(backfillWorkerPool),
          database_(database), request_(request) {}

    void JoinBatchFeatures() override {
        if (batchFeatureThread_.joinable()) batchFeatureThread_.join();
    }

    void StopSameFolderScan() override { sameFolderScanner_.stop(); }
    void StopSearch() override { searchWorker_.stop(); }
    void StopWaveform() override { waveformWorker_.stop(); }
    void StopBackfill() override { backfillWorkerPool_.stop(); }
    void StopCovers() override { CoverModule::ShutdownCoverWallPrecache(); }
    void CloseDatabase() override { database_.close(); }

    bool OpenDatabase(const std::string& pathUtf8) override {
        if (request_.createNew &&
            !replacement_.Prepare(pathUtf8, request_.overwriteExisting)) {
            createError_ = replacement_.Error();
            return false;
        }
        return database_.open(pathUtf8, true);
    }

    void Finish(bool success) {
        if (!success && request_.errorOut) {
            *request_.errorOut = createError_.empty() ? database_.getLastError() : createError_;
        }
        if (!request_.createNew) return;
        if (success) {
            if (!replacement_.Commit()) {
                DIAG_SYSTEM_FMT("Database", "CreateDatabaseBackupRetained: path=%s error=%s",
                    replacement_.RecoveryDatabasePath().c_str(), replacement_.Error().c_str());
            }
            return;
        }
        database_.close();
        if (!replacement_.Rollback()) {
            if (request_.errorOut) *request_.errorOut += "; " + replacement_.Error();
            if (request_.replacingCurrent && request_.recoveryPathOut)
                *request_.recoveryPathOut = replacement_.RecoveryDatabasePath();
        }
    }

    bool SetWaveformCacheMode(int mode) override {
        return database_.setWaveformCacheMode(
            static_cast<WaveformCachePolicy::Mode>(mode));
    }

    int EnqueueStartupReconciliation() override {
        return database_.enqueueStartupReconciliation();
    }

private:
    std::thread& batchFeatureThread_;
    aura::same_folder::SameFolderScanRuntime& sameFolderScanner_;
    SearchWorker& searchWorker_;
    WaveformWorker& waveformWorker_;
    aura::BackfillWorkerPool& backfillWorkerPool_;
    Database& database_;
    const BlockingDatabaseSwitchRequest& request_;
    DatabasePackageReplacement replacement_;
    std::string createError_;
};

} // namespace

bool RunBlockingDatabaseSwitchOperation(
    std::thread& batchFeatureThread,
    aura::same_folder::SameFolderScanRuntime& sameFolderScanner,
    SearchWorker& searchWorker,
    WaveformWorker& waveformWorker,
    aura::BackfillWorkerPool& backfillWorkerPool,
    Database& database,
    const BlockingDatabaseSwitchRequest& request) {
    ConcreteBlockingDatabaseSwitchEnvironment environment(
        batchFeatureThread,
        sameFolderScanner,
        searchWorker,
        waveformWorker,
        backfillWorkerPool,
        database, request);
    const bool success = RunBlockingDatabaseSwitchOperation(environment, request);
    environment.Finish(success);
    return success;
}

} // namespace aura::database_switch

#pragma once

#include <string>
#include <thread>

class Database;
class SearchWorker;
class WaveformWorker;

namespace aura {

class BackfillWorkerPool;

namespace same_folder {
class SameFolderScanRuntime;
}

namespace database_switch {

struct BlockingDatabaseSwitchRequest {
    const std::string& oldDbPathUtf8;
    const std::string& newDbPathUtf8;
    int initialWaveformCacheMode = -1;
    bool autoReconcileOnStartup = false;
    bool createNew = false;
    bool overwriteExisting = false;
    bool replacingCurrent = false;
    std::string* errorOut = nullptr;
    std::string* recoveryPathOut = nullptr;
};

class BlockingDatabaseSwitchEnvironment {
public:
    virtual ~BlockingDatabaseSwitchEnvironment() = default;
    virtual void JoinBatchFeatures() = 0;
    virtual void StopSameFolderScan() = 0;
    virtual void StopSearch() = 0;
    virtual void StopWaveform() = 0;
    virtual void StopBackfill() = 0;
    virtual void StopCovers() = 0;
    virtual void CloseDatabase() = 0;
    virtual bool OpenDatabase(const std::string& pathUtf8) = 0;
    virtual bool SetWaveformCacheMode(int mode) = 0;
    virtual int EnqueueStartupReconciliation() = 0;
};

bool RunBlockingDatabaseSwitchOperation(
    BlockingDatabaseSwitchEnvironment& environment,
    const BlockingDatabaseSwitchRequest& request);

bool RunBlockingDatabaseSwitchOperation(
    std::thread& batchFeatureThread,
    aura::same_folder::SameFolderScanRuntime& sameFolderScanner,
    SearchWorker& searchWorker,
    WaveformWorker& waveformWorker,
    aura::BackfillWorkerPool& backfillWorkerPool,
    Database& database,
    const BlockingDatabaseSwitchRequest& request);

} // namespace database_switch
} // namespace aura

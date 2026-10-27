#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace aura::music_analysis {

struct BpmBatchScanRow {
    std::int64_t fileId = -1;
    std::string filePath;
};

struct BpmBatchScanRequest {
    std::int64_t fileId = -1;
    std::string filePath;
    std::uint64_t generation = 0;
    double startSec = 0.0;
    double endSec = 30.0;
    bool runKey = true;
};

// @stage7-contract
// Private synchronous workflow port. Returned rows and submitted requests own
// their strings; the port is borrowed for one call and retained nowhere.
class BpmBatchScanPort {
public:
    virtual ~BpmBatchScanPort() = default;

    virtual bool databaseOpen() const = 0;
    virtual std::vector<BpmBatchScanRow> queryMissingBpm(
        int limit,
        bool includeFailures,
        const std::string& folderPathPrefix) = 0;
    virtual void cancelP2() = 0;
    virtual void setBatchTotal(int value) = 0;
    virtual void setBatchDone(int value) = 0;
    virtual void setBatchFound(int value) = 0;
    virtual void setBatchRunning(bool value) = 0;
    virtual void setBatchCancelFlag(bool value) = 0;
    virtual std::uint64_t nextGeneration() = 0;
    virtual void submitP2(const BpmBatchScanRequest& request) = 0;
};

bool StartBpmBatchScan(BpmBatchScanPort& port, const std::string& folderPathPrefix = {});

}  // namespace aura::music_analysis

#include "BpmBatchScanWorkflow.h"

namespace aura::music_analysis {

bool StartBpmBatchScan(BpmBatchScanPort& port, const std::string& folderPathPrefix) {
    if (!port.databaseOpen()) return false;
    const auto rows = port.queryMissingBpm(50000, true, folderPathPrefix);
    if (rows.empty()) return false;

    port.cancelP2();
    port.setBatchTotal(static_cast<int>(rows.size()));
    port.setBatchDone(0);
    port.setBatchFound(0);
    port.setBatchRunning(true);
    port.setBatchCancelFlag(false);

    const std::uint64_t generation = port.nextGeneration();
    for (const auto& row : rows) {
        BpmBatchScanRequest request;
        request.fileId = row.fileId;
        request.filePath = row.filePath;
        request.generation = generation;
        request.startSec = 0.0;
        request.endSec = 30.0;
        request.runKey = true;
        port.submitP2(request);
    }
    return true;
}

}  // namespace aura::music_analysis

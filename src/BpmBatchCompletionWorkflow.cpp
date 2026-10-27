#include "BpmBatchCompletionWorkflow.h"

namespace aura::music_analysis {

bool CompleteBpmBatchIfDrained(const BpmBatchCompletionSnapshot& snapshot,
                               BpmBatchCompletionPort& port) {
    if (!snapshot.batchRunning || snapshot.pendingCount != 0 || snapshot.analyzing) {
        return false;
    }
    port.markBatchStopped();
    port.invalidateBrowserMetadataCache();
    return true;
}

}  // namespace aura::music_analysis

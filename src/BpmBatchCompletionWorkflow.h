#pragma once

namespace aura::music_analysis {

struct BpmBatchCompletionSnapshot {
    bool batchRunning = false;
    int pendingCount = 0;
    bool analyzing = false;
};

class BpmBatchCompletionPort {
public:
    virtual ~BpmBatchCompletionPort() = default;
    virtual void markBatchStopped() = 0;
    virtual void invalidateBrowserMetadataCache() = 0;
};

bool CompleteBpmBatchIfDrained(const BpmBatchCompletionSnapshot& snapshot,
                               BpmBatchCompletionPort& port);

}  // namespace aura::music_analysis

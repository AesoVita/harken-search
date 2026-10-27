#include "CoverRebuildBatchCommitWorkflow.h"

namespace aura::cover_rebuild {

void CommitCoverRebuildBatch(
    CoverRebuildBatchCommitInput input,
    CoverRebuildBatchPersistencePort& persistence) {
    if (!input.setHasCoverPaths.empty()) {
        persistence.setHasCover(input.setHasCoverPaths);
    }
    if (!input.clearHasCoverPaths.empty()) {
        persistence.clearHasCover(input.clearHasCoverPaths);
    }
    if (!input.cacheUpdates.empty()) {
        persistence.updateCoverKeyCache(input.cacheUpdates);
    }
}

}  // namespace aura::cover_rebuild

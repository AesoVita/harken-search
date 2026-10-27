#include "AsyncSearchRefreshResultsWorkflow.h"

namespace aura::search {

void HandleAsyncSearchRefreshResults(
    const AsyncSearchRefreshResultsInput& input,
    AsyncSearchRefreshResultsEffects& effects) {
    effects.replaceRows();
    effects.throttleMiniWaveforms(3);
    effects.resultsChanged();
    effects.setBuiltRevision(input.requestedAtRevision);
    effects.setLoadedState(input.loadedCount, input.hasMore);
    effects.clearSelection();
}

}  // namespace aura::search

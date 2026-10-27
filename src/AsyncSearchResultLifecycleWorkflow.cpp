#include "AsyncSearchResultLifecycleWorkflow.h"

namespace aura::search {

bool HandleAsyncSearchResultLifecycle(
    const AsyncSearchResultLifecycleInput& input,
    AsyncSearchResultLifecycleEffects& effects) {
    if (!input.facetRelay) {
        effects.clearPrimarySearchProgress();
    }
    effects.recordQueryDuration(input.queryDurationMs);
    if (!input.success) {
        effects.applyFailureState();
        return true;
    }
    if (input.loadAll && !input.acceptsLoadAll) {
        effects.rejectStaleLoadAll();
        return true;
    }
    return false;
}

}  // namespace aura::search

#include "AsyncSearchRefreshMemoryWorkflow.h"

namespace aura::search {

void HandleAsyncSearchRefreshMemory(
    const AsyncSearchRefreshMemoryInput& input,
    AsyncSearchRefreshMemoryEffects& effects) {
    if (input.hasMemoryBudget) {
        if (input.usage > input.budget) {
            effects.evictLeastRecentlyUsed(input.usage - input.budget);
        }
    } else if (input.fallbackOverLimit) {
        effects.trimFallbackCache();
    }
}

}  // namespace aura::search

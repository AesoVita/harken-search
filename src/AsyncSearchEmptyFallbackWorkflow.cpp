#include "AsyncSearchEmptyFallbackWorkflow.h"

namespace aura::search {

void HandleAsyncSearchEmptyFallback(
    const AsyncSearchEmptyFallbackInput& input,
    AsyncSearchEmptyFallbackEffects& effects) {
    if (input.databaseView && input.resultsEmpty && input.totalCount == 0) {
        effects.clearFallback();
    }
}

}  // namespace aura::search

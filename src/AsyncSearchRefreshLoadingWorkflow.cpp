#include "AsyncSearchRefreshLoadingWorkflow.h"

namespace aura::search {

void HandleAsyncSearchRefreshLoading(
    const AsyncSearchRefreshLoadingInput& input,
    AsyncSearchRefreshLoadingEffects& effects) {
    if (input.refreshResult) effects.finishLoadingMore();
}

}  // namespace aura::search

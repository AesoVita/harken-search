#include "AsyncSearchLoadAllResultStateAdapter.h"

#include "AppState.h"
#include "LoadAllBatchRuntime.h"

namespace aura::search {

AsyncSearchLoadAllResultStateAdapter::AsyncSearchLoadAllResultStateAdapter(
    AppState& state,
    aura::load_all::LoadAllBatchRuntime& runtime)
    : state_(state), runtime_(runtime) {}

bool AsyncSearchLoadAllResultStateAdapter::acceptsResult() const {
    return runtime_.acceptsResult(static_cast<int>(state_.viewMode));
}

void AsyncSearchLoadAllResultStateAdapter::observeResult(
    int loadedCount,
    bool hasMore,
    bool emptyPage) {
    runtime_.onResult(
        static_cast<int>(state_.viewMode),
        loadedCount,
        hasMore,
        emptyPage);
}

}  // namespace aura::search

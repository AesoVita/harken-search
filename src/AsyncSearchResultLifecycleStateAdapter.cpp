#include "AsyncSearchResultLifecycleStateAdapter.h"

#include "AppState.h"

namespace aura::search {

AsyncSearchResultLifecycleStateAdapter::AsyncSearchResultLifecycleStateAdapter(
    AppState& state,
    AsyncSearchResultLifecycleInput input)
    : state_(state), input_(input) {}

bool AsyncSearchResultLifecycleStateAdapter::apply() {
    return HandleAsyncSearchResultLifecycle(input_, *this);
}

void AsyncSearchResultLifecycleStateAdapter::clearPrimarySearchProgress() {
    state_.asyncSearchInProgress = false;
    state_.pendingSearchRequestId = 0;
}

void AsyncSearchResultLifecycleStateAdapter::recordQueryDuration(
    double durationMs) {
    state_.lastFilterQueryMs = durationMs;
}

void AsyncSearchResultLifecycleStateAdapter::applyFailureState() {
    state_.filterQueryState = FilterQueryState::Error;
    state_.filterQueryError = "async search failed";
    state_.loadingMore = false;
    state_.pendingFacetRequestId = 0;
}

void AsyncSearchResultLifecycleStateAdapter::rejectStaleLoadAll() {
    state_.loadingMore = false;
}

}  // namespace aura::search

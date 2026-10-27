#include "AsyncSearchRefreshLoadingStateAdapter.h"

#include "AppState.h"

namespace aura::search {

AsyncSearchRefreshLoadingStateAdapter::
    AsyncSearchRefreshLoadingStateAdapter(AppState& state)
    : state_(state) {}

void AsyncSearchRefreshLoadingStateAdapter::apply() {
    HandleAsyncSearchRefreshLoading({true}, *this);
}

void AsyncSearchRefreshLoadingStateAdapter::finishLoadingMore() {
    state_.loadingMore = false;
}

}  // namespace aura::search

#include "AsyncSearchAppendResultStateAdapter.h"

#include "AppState.h"
#include "AsyncWorker.h"

#include <iterator>
#include <utility>

namespace aura::search {

AsyncSearchAppendResultStateAdapter::AsyncSearchAppendResultStateAdapter(
    AppState& state,
    AsyncSearchResult& result,
    AsyncSearchAppendKind kind,
    ApplyCoverKeyFilter applyCoverKeyFilter,
    LoadAllObserver observeLoadAll)
    : state_(state),
      result_(result),
      kind_(kind),
      applyCoverKeyFilter_(std::move(applyCoverKeyFilter)),
      observeLoadAll_(std::move(observeLoadAll)) {}

void AsyncSearchAppendResultStateAdapter::apply() {
    HandleAsyncSearchAppendResult(
        {kind_,
         result_.loadedCount,
         result_.totalCount,
         result_.hasMore,
         result_.results.empty()},
        *this);
}

void AsyncSearchAppendResultStateAdapter::appendRows() {
    if (result_.results.empty()) return;
    state_.searchResults.reserve(
        state_.searchResults.size() + result_.results.size());
    state_.searchResults.insert(
        state_.searchResults.end(),
        std::make_move_iterator(result_.results.begin()),
        std::make_move_iterator(result_.results.end()));
    state_.loadedCount = static_cast<int>(state_.searchResults.size());
    state_.bumpSearchResultsGen();
}

void AsyncSearchAppendResultStateAdapter::updateCounts(
    std::int64_t totalCount,
    bool hasMore) {
    state_.totalFileCount = totalCount;
    state_.hasMoreResults = hasMore;
}

void AsyncSearchAppendResultStateAdapter::stopAfterEmptyPage() {
    state_.hasMoreResults = false;
}

void AsyncSearchAppendResultStateAdapter::applyCoverKeyFilter() {
    applyCoverKeyFilter_(state_);
}

void AsyncSearchAppendResultStateAdapter::loadAllResult(
    int loadedCount,
    bool hasMore,
    bool emptyPage) {
    observeLoadAll_(loadedCount, hasMore, emptyPage);
}

void AsyncSearchAppendResultStateAdapter::finishLoadingMore() {
    state_.loadingMore = false;
}

}  // namespace aura::search

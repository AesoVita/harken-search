#include "AsyncSearchFacetResultStateAdapter.h"

#include "AppState.h"
#include "AsyncWorker.h"
#include "DiagnosticsLogger.h"

#include <utility>

namespace aura::search {

AsyncSearchFacetResultStateAdapter::AsyncSearchFacetResultStateAdapter(
    AppState& state,
    AsyncSearchResult& result,
    AsyncSearchFacetResultKind kind,
    PersistencePredicate persistenceEligible,
    WarmStartPredicate inconsistentWarmStartTotal,
    RefreshRequest requestRefresh)
    : state_(state),
      result_(result),
      kind_(kind),
      persistenceEligible_(std::move(persistenceEligible)),
      inconsistentWarmStartTotal_(std::move(inconsistentWarmStartTotal)),
      requestRefresh_(std::move(requestRefresh)) {}

void AsyncSearchFacetResultStateAdapter::apply() {
    HandleAsyncSearchFacetResult(
        {kind_,
         result_.success,
         state_.viewMode == ViewMode::Database,
         result_.facetsComputed,
         state_.preservePathFacetsForNextRefresh,
         persistenceEligible_(result_.filter),
         result_.success && inconsistentWarmStartTotal_(),
         state_.pendingSearchRequestId == 0,
         result_.totalCount,
         state_.viewMode == ViewMode::Playlist},
        *this);
}

void AsyncSearchFacetResultStateAdapter::updateTotalCount(std::int64_t total) {
    state_.totalFileCount = total;
}

void AsyncSearchFacetResultStateAdapter::reconcileHasMore() {
    if (state_.totalFileCount >= 0) {
        state_.hasMoreResults = state_.loadedCount < state_.totalFileCount;
    }
}

void AsyncSearchFacetResultStateAdapter::replaceFacets(bool preservePathFacets) {
    std::vector<Database::PathFacetEntry> preservedPathFacets;
    if (preservePathFacets) {
        preservedPathFacets = std::move(state_.currentPathFacets);
    }
    state_.currentCoverFacets = std::move(result_.coverFacets);
    state_.currentPathFacets = preservePathFacets
        ? std::move(preservedPathFacets)
        : std::move(result_.pathFacets);
    if (preservePathFacets) {
        state_.preservePathFacetsForNextRefresh = false;
    }
}

void AsyncSearchFacetResultStateAdapter::persistFacets(bool persistPathFacets) {
    state_.database.persistCoverFacetCache(
        result_.filter, 10000, state_.currentCoverFacets);
    if (persistPathFacets) {
        state_.database.persistPathFacetCache(
            result_.filter, 300, state_.currentPathFacets);
    }
}

void AsyncSearchFacetResultStateAdapter::facetsChanged() {
    state_.bumpFacetsGen();
    requestRefresh_(state_);
}

void AsyncSearchFacetResultStateAdapter::synchronizeView() {
    state_.syncToTab();
}

void AsyncSearchFacetResultStateAdapter::setCountFollowUp(bool pending) {
    state_.pendingCountFollowUp = pending;
    if (pending) {
        DIAG_ACTION_FMT(
            "Search",
            "CountAndFacetWarmStartZeroTotalDeferred: req=%llu keyword=%d folder=%d coverFacets=%zu pathFacets=%zu",
            static_cast<unsigned long long>(result_.requestId),
            result_.filter.keyword.empty() ? 0 : 1,
            result_.filter.folderPath.empty() ? 0 : 1,
            result_.coverFacets.size(),
            result_.pathFacets.size());
    }
}

void AsyncSearchFacetResultStateAdapter::releasePrimarySearch() {
    state_.asyncSearchInProgress = false;
}

void AsyncSearchFacetResultStateAdapter::releaseFacetRequest() {
    state_.pendingFacetRequestId = 0;
}

}  // namespace aura::search

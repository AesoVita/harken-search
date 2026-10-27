#include "AsyncSearchRefreshFacetsAdapter.h"

#include "AppState.h"
#include "AsyncWorker.h"
#include "CoverReplacementFoundation.h"

#include <utility>

namespace aura::search {

AsyncSearchRefreshFacetsAdapter::AsyncSearchRefreshFacetsAdapter(
    AppState& state,
    AsyncSearchResult& result,
    ActiveFilterPredicate hasActiveFilter,
    PersistencePredicate persistenceEligible,
    RefreshRequest requestRefresh)
    : AsyncSearchRefreshFacetsAdapter(
          [&state, &result, hasActiveFilter, persistenceEligible] {
              return AsyncSearchRefreshFacetsInput{
                  state.viewMode == ViewMode::Database,
                  result.loadedCount,
                  result.totalCount,
                  !result.coverFacets.empty(),
                  !result.pathFacets.empty(),
                  state.coverFilterActive &&
                      !state.coverFilterPath.empty(),
                  state.pendingFacetRequestId == result.requestId,
                  hasActiveFilter(state),
                  state.preservePathFacetsForNextRefresh,
                  persistenceEligible(result.filter),
                  state.viewMode == ViewMode::Playlist};
          },
          [&state, &result](bool preservePathFacets) {
              std::vector<Database::PathFacetEntry> preserved;
              if (preservePathFacets) {
                  preserved = std::move(state.currentPathFacets);
              }
              state.currentCoverFacets = std::move(result.coverFacets);
              state.currentPathFacets = preservePathFacets
                  ? std::move(preserved)
                  : std::move(result.pathFacets);
              if (preservePathFacets) {
                  state.preservePathFacetsForNextRefresh = false;
              }
          },
          [&state] {
              state.currentCoverFacets.clear();
              state.currentPathFacets.clear();
          },
          [&state, &result](bool persistPathFacets) {
              state.database.persistCoverFacetCache(
                  result.filter, 10000, state.currentCoverFacets);
              if (persistPathFacets) {
                  state.database.persistPathFacetCache(
                      result.filter, 300, state.currentPathFacets);
              }
          },
          [&state, &result, requestRefresh = std::move(requestRefresh)] {
              state.bumpFacetsGen();
              requestRefresh(state);
              if (state.viewMode == ViewMode::Playlist &&
                  result.kind == AsyncSearchKind::Refresh &&
                  state.pendingFacetRequestId == result.requestId) {
                  state.pendingFacetRequestId = 0;
              }
          }) {}

}  // namespace aura::search

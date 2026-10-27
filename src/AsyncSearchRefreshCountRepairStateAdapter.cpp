#include "AsyncSearchRefreshCountRepairAdapter.h"

#include "AppState.h"
#include "AsyncWorker.h"
#include "DiagnosticsLogger.h"

#include <utility>

namespace aura::search {

AsyncSearchRefreshCountRepairAdapter::AsyncSearchRefreshCountRepairAdapter(
    AppState& state,
    AsyncSearchResult& result,
    ActiveFilterPredicate hasActiveFilter,
    StateRepairSubmitter submitRepair)
    : AsyncSearchRefreshCountRepairAdapter(
          [&state, &result, hasActiveFilter] {
              return AsyncSearchRefreshCountRepairInput{
                  result.totalCount == -1 && !result.countFollowUpSkipped,
                  result.loadedCount,
                  result.totalCount,
                  state.viewMode == ViewMode::Database,
                  state.pendingFacetRequestId != 0,
                  hasActiveFilter(state),
                  state.coverFilterActive};
          },
          [&state] { state.pendingCountFollowUp = true; },
          [&state, &result, submitRepair = std::move(submitRepair)](
              bool needRepairFacets) {
              const auto repairId =
                  submitRepair(state, result.filter, needRepairFacets);
              DIAG_ACTION_FMT(
                  "Search",
                  "InconsistentRefreshTotalRepair: oldReq=%llu repairReq=%llu loaded=%d total=%lld keyword=%d folder=%d",
                  static_cast<unsigned long long>(result.requestId),
                  static_cast<unsigned long long>(repairId),
                  result.loadedCount,
                  static_cast<long long>(result.totalCount),
                  result.filter.keyword.empty() ? 0 : 1,
                  result.filter.folderPath.empty() ? 0 : 1);
          },
          [&state, &result] {
              DIAG_ACTION_FMT(
                  "Search",
                  "InconsistentRefreshTotalPending: req=%llu loaded=%d total=%lld pendingFacet=%llu",
                  static_cast<unsigned long long>(result.requestId),
                  result.loadedCount,
                  static_cast<long long>(result.totalCount),
                  static_cast<unsigned long long>(
                      state.pendingFacetRequestId));
          },
          [&state](std::int64_t totalCount) {
              state.totalFileCount = totalCount;
          }) {}

}  // namespace aura::search

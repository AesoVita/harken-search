#include "AsyncSearchRefreshDiagnosticsStateAdapter.h"

#include "AppState.h"
#include "DiagnosticsLogger.h"

namespace aura::search {

namespace {

class Effects final : public AsyncSearchRefreshDiagnosticsEffects {
public:
    void recordMetrics(
        double queryDurationMs,
        double resultCount,
        double totalCount) override {
        DIAG_METRIC("Search", "RefreshSearchResults", queryDurationMs);
        DIAG_METRIC("Search", "ResultCount", resultCount);
        DIAG_METRIC("Search", "TotalCount", totalCount);
    }
    void reportEmptyQuery(const std::string&) override {
        DIAG_ACTION("Search.Empty", "No results for query");
    }
};

}  // namespace

AsyncSearchRefreshDiagnosticsStateAdapter::
    AsyncSearchRefreshDiagnosticsStateAdapter(AppState& state)
    : state_(state) {}

void AsyncSearchRefreshDiagnosticsStateAdapter::apply() {
    Effects effects;
    HandleAsyncSearchRefreshDiagnostics(
        {state_.viewMode == ViewMode::Database,
         state_.activeSmartCollection == SmartCollection::None,
         state_.activeUserCollectionIdx,
         state_.lastAppliedSearchText,
         state_.searchResults.empty(),
         state_.asyncSearchInProgress,
         state_.lastFilterQueryMs,
         state_.searchResults.size(),
         state_.totalFileCount},
        effects);
}

}  // namespace aura::search

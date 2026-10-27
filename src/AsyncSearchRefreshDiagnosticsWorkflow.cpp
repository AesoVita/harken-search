#include "AsyncSearchRefreshDiagnosticsWorkflow.h"

namespace aura::search {

void HandleAsyncSearchRefreshDiagnostics(
    const AsyncSearchRefreshDiagnosticsInput& input,
    AsyncSearchRefreshDiagnosticsEffects& effects) {
    effects.recordMetrics(
        input.queryDurationMs,
        static_cast<double>(input.resultCount),
        static_cast<double>(input.totalCount));
    if (input.databaseView &&
        input.smartCollectionNone &&
        input.activeUserCollectionIndex < 0 &&
        !input.query.empty() &&
        input.resultsEmpty &&
        !input.asyncSearchInProgress) {
        effects.reportEmptyQuery(input.query);
    }
}

}  // namespace aura::search

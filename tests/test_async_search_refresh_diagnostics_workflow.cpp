#include "AsyncSearchRefreshDiagnosticsWorkflow.h"

#include <cassert>

struct Effects final : aura::search::AsyncSearchRefreshDiagnosticsEffects {
    int metrics = 0;
    int emptyReports = 0;
    void recordMetrics(double, double, double) override { ++metrics; }
    void reportEmptyQuery(const std::string&) override { ++emptyReports; }
};

int main() {
    Effects empty;
    aura::search::AsyncSearchRefreshDiagnosticsInput input;
    input.databaseView = true;
    input.smartCollectionNone = true;
    input.activeUserCollectionIndex = -1;
    input.query = "footstep";
    input.resultsEmpty = true;
    aura::search::HandleAsyncSearchRefreshDiagnostics(input, empty);
    assert(empty.metrics == 1 && empty.emptyReports == 1);

    Effects collection;
    input.activeUserCollectionIndex = 0;
    aura::search::HandleAsyncSearchRefreshDiagnostics(input, collection);
    assert(collection.metrics == 1 && collection.emptyReports == 0);

    Effects active;
    input.activeUserCollectionIndex = -1;
    input.asyncSearchInProgress = true;
    aura::search::HandleAsyncSearchRefreshDiagnostics(input, active);
    assert(active.metrics == 1 && active.emptyReports == 0);
    return 0;
}

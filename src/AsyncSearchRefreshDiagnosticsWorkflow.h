#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace aura::search {

struct AsyncSearchRefreshDiagnosticsInput {
    bool databaseView = false;
    bool smartCollectionNone = false;
    int activeUserCollectionIndex = -1;
    std::string query;
    bool resultsEmpty = false;
    bool asyncSearchInProgress = false;
    double queryDurationMs = 0.0;
    std::size_t resultCount = 0;
    std::int64_t totalCount = 0;
};

class AsyncSearchRefreshDiagnosticsEffects {
public:
    virtual ~AsyncSearchRefreshDiagnosticsEffects() = default;
    virtual void recordMetrics(
        double queryDurationMs,
        double resultCount,
        double totalCount) = 0;
    virtual void reportEmptyQuery(const std::string& query) = 0;
};

void HandleAsyncSearchRefreshDiagnostics(
    const AsyncSearchRefreshDiagnosticsInput& input,
    AsyncSearchRefreshDiagnosticsEffects& effects);

}  // namespace aura::search

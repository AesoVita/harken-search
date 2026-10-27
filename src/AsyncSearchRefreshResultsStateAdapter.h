#pragma once

#include "AsyncSearchRefreshResultsWorkflow.h"

struct AppState;
struct AsyncSearchResult;

namespace aura::search {

class AsyncSearchRefreshResultsStateAdapter final {
public:
    AsyncSearchRefreshResultsStateAdapter(
        AppState& state,
        AsyncSearchResult& result);

    void apply();

private:
    AppState& state_;
    AsyncSearchResult& result_;
};

}  // namespace aura::search

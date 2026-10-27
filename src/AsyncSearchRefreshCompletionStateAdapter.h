#pragma once

#include "AsyncSearchRefreshCompletionWorkflow.h"

#include <functional>

struct AppState;

namespace aura::search {

class AsyncSearchRefreshCompletionStateAdapter final {
public:
    using AdvancedFilterPredicate = std::function<bool(const AppState&)>;
    using RefreshRequest = std::function<void(AppState&)>;

    AsyncSearchRefreshCompletionStateAdapter(
        AppState& state,
        AdvancedFilterPredicate hasAdvancedFilters,
        RefreshRequest requestCoverWallRefresh);

    void apply();

private:
    AppState& state_;
    AdvancedFilterPredicate hasAdvancedFilters_;
    RefreshRequest requestCoverWallRefresh_;
};

}  // namespace aura::search

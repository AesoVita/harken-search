#pragma once

#include "AsyncSearchCoverPreloadWorkflow.h"

#include <functional>

struct AppState;

namespace aura::search {

class AsyncSearchCoverPreloadStateAdapter final {
public:
    using AdvancedFilterPredicate = std::function<bool(const AppState&)>;
    using CoverWallPriorityPredicate = std::function<bool()>;

    AsyncSearchCoverPreloadStateAdapter(
        AppState& state,
        AdvancedFilterPredicate hasAdvancedFilters,
        CoverWallPriorityPredicate isCoverWallPriorityMode);

    void apply();

private:
    AppState& state_;
    AdvancedFilterPredicate hasAdvancedFilters_;
    CoverWallPriorityPredicate isCoverWallPriorityMode_;
};

}  // namespace aura::search

#pragma once

#include "AsyncSearchRefreshLoadingWorkflow.h"

struct AppState;

namespace aura::search {

class AsyncSearchRefreshLoadingStateAdapter final
    : private AsyncSearchRefreshLoadingEffects {
public:
    explicit AsyncSearchRefreshLoadingStateAdapter(AppState& state);

    void apply();

private:
    void finishLoadingMore() override;

    AppState& state_;
};

}  // namespace aura::search

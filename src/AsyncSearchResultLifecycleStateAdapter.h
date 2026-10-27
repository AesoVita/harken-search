#pragma once

#include "AsyncSearchResultLifecycleWorkflow.h"

struct AppState;

namespace aura::search {

class AsyncSearchResultLifecycleStateAdapter final
    : private AsyncSearchResultLifecycleEffects {
public:
    AsyncSearchResultLifecycleStateAdapter(
        AppState& state,
        AsyncSearchResultLifecycleInput input);

    bool apply();

private:
    void clearPrimarySearchProgress() override;
    void recordQueryDuration(double durationMs) override;
    void applyFailureState() override;
    void rejectStaleLoadAll() override;

    AppState& state_;
    AsyncSearchResultLifecycleInput input_;
};

}  // namespace aura::search

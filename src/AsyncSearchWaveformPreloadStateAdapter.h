#pragma once

#include "AsyncSearchWaveformPreloadWorkflow.h"

struct AppState;

namespace aura::search {

class AsyncSearchWaveformPreloadStateAdapter final
    : private AsyncSearchWaveformPreloadEffects {
public:
    AsyncSearchWaveformPreloadStateAdapter(
        AppState& state,
        bool columnVisible);

    void apply();

private:
    void enqueueResult(std::size_t index) override;

    AppState& state_;
    bool columnVisible_ = false;
};

}  // namespace aura::search

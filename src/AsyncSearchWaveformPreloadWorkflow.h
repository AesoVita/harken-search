#pragma once

#include <cstddef>

namespace aura::search {

struct AsyncSearchWaveformPreloadInput {
    bool columnVisible = false;
    int settleFramesRemaining = 0;
    std::size_t resultCount = 0;
};

class AsyncSearchWaveformPreloadEffects {
public:
    virtual ~AsyncSearchWaveformPreloadEffects() = default;
    virtual void enqueueResult(std::size_t index) = 0;
};

void HandleAsyncSearchWaveformPreload(
    const AsyncSearchWaveformPreloadInput& input,
    AsyncSearchWaveformPreloadEffects& effects);

}  // namespace aura::search

#pragma once

namespace aura::search {

struct AsyncSearchResultLifecycleInput {
    bool facetRelay = false;
    bool success = false;
    bool loadAll = false;
    bool acceptsLoadAll = true;
    double queryDurationMs = 0.0;
};

class AsyncSearchResultLifecycleEffects {
public:
    virtual ~AsyncSearchResultLifecycleEffects() = default;
    virtual void clearPrimarySearchProgress() = 0;
    virtual void recordQueryDuration(double durationMs) = 0;
    virtual void applyFailureState() = 0;
    virtual void rejectStaleLoadAll() = 0;
};

bool HandleAsyncSearchResultLifecycle(
    const AsyncSearchResultLifecycleInput& input,
    AsyncSearchResultLifecycleEffects& effects);

}  // namespace aura::search

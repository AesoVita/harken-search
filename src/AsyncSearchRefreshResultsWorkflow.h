#pragma once

#include <cstdint>

namespace aura::search {

struct AsyncSearchRefreshResultsInput {
    int loadedCount = 0;
    bool hasMore = false;
    std::uint64_t requestedAtRevision = 0;
};

class AsyncSearchRefreshResultsEffects {
public:
    virtual ~AsyncSearchRefreshResultsEffects() = default;
    virtual void replaceRows() = 0;
    virtual void throttleMiniWaveforms(int frames) = 0;
    virtual void resultsChanged() = 0;
    virtual void setBuiltRevision(std::uint64_t revision) = 0;
    virtual void setLoadedState(int loadedCount, bool hasMore) = 0;
    virtual void clearSelection() = 0;
};

void HandleAsyncSearchRefreshResults(
    const AsyncSearchRefreshResultsInput& input,
    AsyncSearchRefreshResultsEffects& effects);

}  // namespace aura::search

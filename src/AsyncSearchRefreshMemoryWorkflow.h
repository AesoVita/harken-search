#pragma once

#include <cstdint>

namespace aura::search {

struct AsyncSearchRefreshMemoryInput {
    bool hasMemoryBudget = false;
    std::uint64_t usage = 0;
    std::uint64_t budget = 0;
    bool fallbackOverLimit = false;
};

class AsyncSearchRefreshMemoryEffects {
public:
    virtual ~AsyncSearchRefreshMemoryEffects() = default;
    virtual void evictLeastRecentlyUsed(std::uint64_t overBudget) = 0;
    virtual void trimFallbackCache() = 0;
};

void HandleAsyncSearchRefreshMemory(
    const AsyncSearchRefreshMemoryInput& input,
    AsyncSearchRefreshMemoryEffects& effects);

}  // namespace aura::search

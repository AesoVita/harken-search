#include "AsyncSearchRefreshMemoryWorkflow.h"

#include <cassert>

struct Effects final : aura::search::AsyncSearchRefreshMemoryEffects {
    int evict = 0;
    int trim = 0;
    void evictLeastRecentlyUsed(std::uint64_t) override { ++evict; }
    void trimFallbackCache() override { ++trim; }
};

int main() {
    Effects budget;
    aura::search::HandleAsyncSearchRefreshMemory({true, 20, 10, true}, budget);
    assert(budget.evict == 1 && budget.trim == 0);
    Effects fallback;
    aura::search::HandleAsyncSearchRefreshMemory({false, 0, 0, true}, fallback);
    assert(fallback.evict == 0 && fallback.trim == 1);
    return 0;
}

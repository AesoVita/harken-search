#include "AsyncSearchWaveformPreloadWorkflow.h"

#include <cassert>
#include <vector>

struct Effects final : aura::search::AsyncSearchWaveformPreloadEffects {
    std::vector<std::size_t> indices;
    void enqueueResult(std::size_t index) override { indices.push_back(index); }
};

int main() {
    Effects active;
    aura::search::HandleAsyncSearchWaveformPreload({true, 0, 8}, active);
    assert((active.indices == std::vector<std::size_t>{0, 1, 2, 3, 4}));
    Effects paused;
    aura::search::HandleAsyncSearchWaveformPreload({true, 1, 8}, paused);
    assert(paused.indices.empty());
    return 0;
}

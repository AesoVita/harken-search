#include "AsyncSearchRefreshResultsWorkflow.h"

#include <cassert>
#include <vector>

struct Effects final : aura::search::AsyncSearchRefreshResultsEffects {
    std::vector<int> events;
    void replaceRows() override { events.push_back(1); }
    void throttleMiniWaveforms(int) override { events.push_back(2); }
    void resultsChanged() override { events.push_back(3); }
    void setBuiltRevision(std::uint64_t) override { events.push_back(4); }
    void setLoadedState(int, bool) override { events.push_back(5); }
    void clearSelection() override { events.push_back(6); }
};

int main() {
    Effects effects;
    aura::search::HandleAsyncSearchRefreshResults({10, true, 42}, effects);
    assert((effects.events == std::vector<int>{1, 2, 3, 4, 5, 6}));
    return 0;
}

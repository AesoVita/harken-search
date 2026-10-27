#include "AsyncSearchRefreshCountRepairWorkflow.h"

#include <cassert>
#include <vector>

struct Effects final : aura::search::AsyncSearchRefreshCountRepairEffects {
    std::vector<int> events;
    void markCountFollowUpPending() override { events.push_back(1); }
    void submitRepair(bool facets) override { events.push_back(facets ? 2 : 3); }
    void reportRepairPending() override { events.push_back(4); }
};

int main() {
    Effects unknown;
    aura::search::AsyncSearchRefreshCountRepairInput unknownInput;
    unknownInput.totalUnknown = true;
    unknownInput.databaseView = true;
    unknownInput.hasActiveDbFilter = true;
    assert(aura::search::HandleAsyncSearchRefreshCountRepair(unknownInput, unknown));
    assert((unknown.events == std::vector<int>{1}));
    Effects repair;
    aura::search::AsyncSearchRefreshCountRepairInput repairInput;
    repairInput.loadedCount = 2;
    repairInput.totalCount = 0;
    repairInput.databaseView = true;
    repairInput.hasActiveDbFilter = true;
    assert(aura::search::HandleAsyncSearchRefreshCountRepair(repairInput, repair));
    assert((repair.events == std::vector<int>{1, 2}));
    Effects pending;
    repairInput.repairAlreadyPending = true;
    assert(aura::search::HandleAsyncSearchRefreshCountRepair(repairInput, pending));
    assert((pending.events == std::vector<int>{1, 4}));
    return 0;
}

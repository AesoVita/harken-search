#pragma once

#include <cstdint>

namespace aura::search {

struct AsyncSearchRefreshCountRepairInput {
    bool totalUnknown = false;
    int loadedCount = 0;
    std::int64_t totalCount = 0;
    bool databaseView = false;
    bool repairAlreadyPending = false;
    bool hasActiveDbFilter = false;
    bool coverFilterActive = false;
};

class AsyncSearchRefreshCountRepairEffects {
public:
    virtual ~AsyncSearchRefreshCountRepairEffects() = default;
    virtual void markCountFollowUpPending() = 0;
    virtual void submitRepair(bool needRepairFacets) = 0;
    virtual void reportRepairPending() = 0;
};

bool HandleAsyncSearchRefreshCountRepair(
    const AsyncSearchRefreshCountRepairInput& input,
    AsyncSearchRefreshCountRepairEffects& effects);

}  // namespace aura::search

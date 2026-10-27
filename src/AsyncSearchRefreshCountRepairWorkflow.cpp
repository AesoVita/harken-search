#include "AsyncSearchRefreshCountRepairWorkflow.h"

namespace aura::search {

bool HandleAsyncSearchRefreshCountRepair(
    const AsyncSearchRefreshCountRepairInput& input,
    AsyncSearchRefreshCountRepairEffects& effects) {
    const bool inconsistentZeroTotal =
        input.databaseView && input.loadedCount > 0 &&
        input.totalCount == 0 && input.hasActiveDbFilter;
    if (!input.totalUnknown && !inconsistentZeroTotal) return false;
    effects.markCountFollowUpPending();
    if (!inconsistentZeroTotal) return true;
    if (input.repairAlreadyPending) {
        effects.reportRepairPending();
    } else {
        effects.submitRepair(input.hasActiveDbFilter && !input.coverFilterActive);
    }
    return true;
}

}  // namespace aura::search

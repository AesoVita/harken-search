#include "CoverRebuildLifecycleWorkflow.h"

namespace aura::cover_rebuild {

bool ResolveCoverRebuildLifecycle(
    CoverRebuildLifecycleInput input,
    CoverRebuildLifecycleStatePort& state,
    CoverRebuildLifecycleEffectsPort& effects) {
    if (input.cancelRequested) {
        state.cancelBatch();
        state.clearFacets();
        effects.forceCoverWallReload();
        state.setPreferDeferredCount();
        effects.refreshSearchResults();
        return true;
    }
    if (!input.batchEmpty) return false;

    state.completeBatch();
    state.switchToCoverTab();
    if (input.coverRegistryStale) effects.syncCoverRegistry();
    state.clearFacets();
    effects.forceCoverWallReload();
    state.setPreferDeferredCount();
    effects.refreshSearchResults();
    return true;
}

}  // namespace aura::cover_rebuild

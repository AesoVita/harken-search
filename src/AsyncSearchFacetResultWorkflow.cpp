#include "AsyncSearchFacetResultWorkflow.h"

namespace aura::search {

bool HandleAsyncSearchFacetResult(const AsyncSearchFacetResultInput& input,
                                  AsyncSearchFacetResultEffects& effects) {
    if (input.kind == AsyncSearchFacetResultKind::FacetsOnly) {
        if (input.success && (input.databaseView || input.playlistView)) {
            effects.replaceFacets(input.preservePathFacets);
            if (input.persistenceEligible && input.databaseView) {
                effects.persistFacets(!input.preservePathFacets);
            }
            effects.facetsChanged();
            effects.synchronizeView();
        }
        effects.releaseFacetRequest();
        return true;
    }

    if (input.kind != AsyncSearchFacetResultKind::CountAndFacetsOnly) return false;

    if (input.success && (input.databaseView || input.playlistView)) {
        if (!input.inconsistentWarmStartTotal) {
            effects.updateTotalCount(input.totalCount);
            effects.reconcileHasMore();
        }
        if (input.facetsComputed) {
            effects.replaceFacets(input.preservePathFacets);
            if (input.persistenceEligible && input.databaseView) {
                effects.persistFacets(!input.preservePathFacets);
            }
            effects.facetsChanged();
        }
        effects.synchronizeView();
    }
    effects.setCountFollowUp(input.inconsistentWarmStartTotal);
    if (input.standaloneCountFollowUp) effects.releasePrimarySearch();
    effects.releaseFacetRequest();
    return true;
}

}  // namespace aura::search

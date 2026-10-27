#include "AsyncSearchRefreshFacetsWorkflow.h"

namespace aura::search {

bool HandleAsyncSearchRefreshFacets(
    const AsyncSearchRefreshFacetsInput& input,
    AsyncSearchRefreshFacetsEffects& effects) {
    if (input.playlistView && input.hasCoverFacetPayload) {
        effects.replaceFacets(input.preservePathFacets);
        effects.facetsChanged();
        return true;
    }
    if (!input.databaseView) {
        effects.clearFacets();
        effects.facetsChanged();
        return true;
    }
    const bool hasFacetPayload =
        input.hasCoverFacetPayload || input.hasPathFacetPayload;
    const bool definitelyNoResults = input.loadedCount == 0 && input.totalCount == 0;
    const bool awaitingFacetFollowUp =
        !input.coverFilterActive && input.pendingFacetMatchesRequest;
    const bool shouldDropStaleFacetsForFallback =
        awaitingFacetFollowUp &&
        !hasFacetPayload &&
        !definitelyNoResults &&
        input.hasActiveDbFilter;
    const bool shouldApply =
        hasFacetPayload || definitelyNoResults || input.coverFilterActive ||
        shouldDropStaleFacetsForFallback;
    if (!shouldApply) return false;
    effects.replaceFacets(input.preservePathFacets);
    if (hasFacetPayload && input.persistenceEligible) {
        effects.persistFacets(!input.preservePathFacets);
    }
    effects.facetsChanged();
    return true;
}

}  // namespace aura::search

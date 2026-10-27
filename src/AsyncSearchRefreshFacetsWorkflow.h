#pragma once

#include <cstdint>

namespace aura::search {

struct AsyncSearchRefreshFacetsInput {
    bool databaseView = false;
    int loadedCount = 0;
    std::int64_t totalCount = 0;
    bool hasCoverFacetPayload = false;
    bool hasPathFacetPayload = false;
    bool coverFilterActive = false;
    bool pendingFacetMatchesRequest = false;
    bool hasActiveDbFilter = false;
    bool preservePathFacets = false;
    bool persistenceEligible = false;
    bool playlistView = false;
};

class AsyncSearchRefreshFacetsEffects {
public:
    virtual ~AsyncSearchRefreshFacetsEffects() = default;
    virtual void replaceFacets(bool preservePathFacets) = 0;
    virtual void clearFacets() = 0;
    virtual void persistFacets(bool persistPathFacets) = 0;
    virtual void facetsChanged() = 0;
};

bool HandleAsyncSearchRefreshFacets(
    const AsyncSearchRefreshFacetsInput& input,
    AsyncSearchRefreshFacetsEffects& effects);

}  // namespace aura::search

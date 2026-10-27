#pragma once

#include <cstdint>

namespace aura::search {

enum class AsyncSearchFacetResultKind {
    Other,
    FacetsOnly,
    CountAndFacetsOnly,
};

struct AsyncSearchFacetResultInput {
    AsyncSearchFacetResultKind kind = AsyncSearchFacetResultKind::Other;
    bool success = false;
    bool databaseView = false;
    bool facetsComputed = false;
    bool preservePathFacets = false;
    bool persistenceEligible = false;
    bool inconsistentWarmStartTotal = false;
    bool standaloneCountFollowUp = false;
    std::int64_t totalCount = 0;
    bool playlistView = false;
};

class AsyncSearchFacetResultEffects {
public:
    virtual ~AsyncSearchFacetResultEffects() = default;
    virtual void updateTotalCount(std::int64_t) = 0;
    virtual void reconcileHasMore() = 0;
    virtual void replaceFacets(bool preservePathFacets) = 0;
    virtual void persistFacets(bool persistPathFacets) = 0;
    virtual void facetsChanged() = 0;
    virtual void synchronizeView() = 0;
    virtual void setCountFollowUp(bool pending) = 0;
    virtual void releasePrimarySearch() = 0;
    virtual void releaseFacetRequest() = 0;
};

bool HandleAsyncSearchFacetResult(const AsyncSearchFacetResultInput&,
                                  AsyncSearchFacetResultEffects&);

}  // namespace aura::search

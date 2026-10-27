#include "AsyncSearchFacetWarmStartPolicy.h"

#include "CoverReplacementFoundation.h"

namespace aura::search {

bool IsFilteredFacetWarmStartCountStateImpossible(
    std::int64_t totalCount,
    bool activeDatabaseFilter,
    bool facetsComputed,
    const std::vector<Database::CoverFacetEntry>& coverFacets,
    const std::vector<Database::PathFacetEntry>& pathFacets) {
    return aura::cover_replacement::IsFilteredFacetWarmStartCountStateImpossible(
        totalCount,
        activeDatabaseFilter,
        facetsComputed,
        !coverFacets.empty() || !pathFacets.empty());
}

}  // namespace aura::search

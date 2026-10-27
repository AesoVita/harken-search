#pragma once

#include <cstdint>
#include <vector>

#include "Database.h"

namespace aura::search {

bool IsFilteredFacetWarmStartCountStateImpossible(
    std::int64_t totalCount,
    bool activeDatabaseFilter,
    bool facetsComputed,
    const std::vector<Database::CoverFacetEntry>& coverFacets,
    const std::vector<Database::PathFacetEntry>& pathFacets);

}  // namespace aura::search

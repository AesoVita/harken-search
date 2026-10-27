#pragma once

#include "CoverModule.h"
#include "Database.h"

#include <vector>

namespace aura::cover_wall {

void BuildCoverWallItemsFromCoverFacets(
    const std::vector<Database::CoverFacetEntry>& facets,
    std::vector<CoverModule::CoverItem>& outItems);

}  // namespace aura::cover_wall

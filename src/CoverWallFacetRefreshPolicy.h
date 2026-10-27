#pragma once

#include "CoverModule.h"

#include <vector>

namespace aura::cover_wall {

bool CoverWallItemsShareVisualOrder(
    const std::vector<CoverModule::CoverItem>& existing,
    const std::vector<CoverModule::CoverItem>& updated);
void RefreshCoverWallFacetMetadataInPlace(
    std::vector<CoverModule::CoverItem>& existing,
    const std::vector<CoverModule::CoverItem>& updated);

}  // namespace aura::cover_wall

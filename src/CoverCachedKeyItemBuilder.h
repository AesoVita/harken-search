#pragma once

#include "CoverModule.h"
#include "Database.h"

#include <cstddef>
#include <vector>

namespace aura::cover_wall {

inline constexpr size_t kMaxRecordsForCoverBuildOnUIThread = 50000;

bool IsRealCoverWallItemCandidate(const CoverModule::CoverItem& item);
void SanitizeCoverWallItemsForCache(std::vector<CoverModule::CoverItem>& items);
void BuildCoverWallItemsFromCachedKeys(
    const std::vector<AudioRecord>& records,
    std::vector<CoverModule::CoverItem>& outItems);
void BuildCoverWallItemsFromSearchResultsFallback(
    const std::vector<AudioRecord>& searchResults,
    std::vector<CoverModule::CoverItem>& outItems);

}  // namespace aura::cover_wall

#pragma once

#include <cstddef>

namespace HarkenSearch::WaveformPrewarm {

// @stage7-contract
// API contract: prewarm policy helpers are pure, noexcept scalar predicates.
// They do not inspect or mutate cache state; callers provide the observed
// cached value count and interpret the established boolean threshold.

inline constexpr int kDetailPrewarmPeaks = 3000;

inline bool HasUsableDetailCache(std::size_t cachedFloatCount) noexcept
{
    return cachedFloatCount > 0;
}

} // namespace HarkenSearch::WaveformPrewarm

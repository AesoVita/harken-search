#pragma once

#include <cstdint>

struct AppState;
struct SearchFilter;

namespace aura::search {

class AsyncSearchQueryTimingRegistry;

std::uint64_t SubmitCountAndFacetRepair(
    AppState& state,
    AsyncSearchQueryTimingRegistry& queryTiming,
    const SearchFilter& filter,
    bool needFacets,
    bool hasAdvancedFilters,
    bool hasFileTypeFilter);

}  // namespace aura::search


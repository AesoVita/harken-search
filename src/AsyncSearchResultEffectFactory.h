#pragma once

#include "AsyncSearchResultApplicationAdapter.h"

namespace aura::search {

class AsyncSearchQueryTimingRegistry;

struct AsyncSearchResultEffectPorts final {
    AsyncSearchRefreshResultCompositionEffects::ReadStatePredicate
        readIntegratedMode;
    AsyncSearchRefreshResultCompositionEffects::ReadStatePredicate
        hasActiveDatabaseFilter;
    AsyncSearchRefreshResultCompositionEffects::ReadStatePredicate
        hasAdvancedFilters;
    AsyncSearchRefreshResultCompositionEffects::ReadFlag
        isCoverWallPriorityMode;
    AsyncSearchRefreshResultCompositionEffects::ResetWaveformZoom
        resetWaveformZoom;
    AsyncSearchRefreshResultCompositionEffects::ReleaseWaveformEvicted
        releaseWaveformEvicted;
};

AsyncSearchResultApplicationEffects BuildApplicationEffects(
    AsyncSearchQueryTimingRegistry& queryTiming,
    AsyncSearchResultEffectPorts ports);

}  // namespace aura::search

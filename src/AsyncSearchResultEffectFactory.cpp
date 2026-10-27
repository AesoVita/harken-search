#include "AsyncSearchResultEffectFactory.h"

#include "AppState.h"
#include "AsyncSearchCountFacetRepairSubmission.h"
#include "AsyncSearchCoverPrimingRuntimeAdapter.h"
#include "AsyncSearchFacetWarmStartPolicy.h"
#include "AsyncSearchQueryTimingRegistry.h"
#include "AsyncSearchResultFilterAdapter.h"
#include "AsyncWorker.h"
#include "CoverKeyFilterStateAdapter.h"
#include "CoverWallRefreshStateAdapter.h"
#include "PathUtils.h"
#include "PlaybackHistoryStateAdapter.h"
#include "WaveformDetailSchedulingStateAdapter.h"

#include <utility>

namespace aura::search {

AsyncSearchResultApplicationEffects BuildApplicationEffects(
    AsyncSearchQueryTimingRegistry& queryTiming,
    AsyncSearchResultEffectPorts ports) {
    auto activeDatabaseFilter = ports.hasActiveDatabaseFilter;
    auto advancedFilters = ports.hasAdvancedFilters;

    return {
        ApplyPlaylistScopedFilters,
        FilterResultsByFileTypePolicy,
        [activeDatabaseFilter](const AppState& state,
                               const AsyncSearchResult& result) {
            return IsFilteredFacetWarmStartCountStateImpossible(
                result.totalCount,
                state.viewMode == ViewMode::Database &&
                    activeDatabaseFilter(state),
                result.facetsComputed,
                result.coverFacets,
                result.pathFacets);
        },
        aura::cover_wall::RequestDisplayRefresh,
        ApplyCoverKeyFilter,
        {
            StartDeferredCoverPriming,
            ApplyCoverKeyFilter,
            std::move(ports.readIntegratedMode),
            activeDatabaseFilter,
            HarkenSearch::CanonicalFolderPath,
            activeDatabaseFilter,
            advancedFilters,
            std::move(ports.isCoverWallPriorityMode),
            [&queryTiming, advancedFilters](
                AppState& state,
                const SearchFilter& filter,
                bool needFacets) {
                return SubmitCountAndFacetRepair(
                    state,
                    queryTiming,
                    filter,
                    needFacets,
                    advancedFilters(state),
                    !(state.showWAV && state.showMP3 && state.showFLAC &&
                      state.showOGG && state.showAIFF && state.showM4A &&
                      state.showWMA && state.showOther));
            },
            aura::cover_wall::RequestDisplayRefresh,
            aura::waveform::ScheduleDeferredDetail,
            std::move(ports.resetWaveformZoom),
            aura::playback::MarkFilePlayed,
            std::move(ports.releaseWaveformEvicted),
        },
    };
}

}  // namespace aura::search

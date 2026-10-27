#include "AsyncSearchCountFacetRepairSubmission.h"

#include "AppState.h"
#include "AsyncSearchQueryTimingRegistry.h"
#include "AsyncWorker.h"

namespace aura::search {

std::uint64_t SubmitCountAndFacetRepair(
    AppState& state,
    AsyncSearchQueryTimingRegistry& queryTiming,
    const SearchFilter& filter,
    bool needFacets,
    bool hasAdvancedFilters,
    bool hasFileTypeFilter) {
    AsyncSearchRequest req;
    req.filter = filter;
    req.viewMode = static_cast<int>(state.viewMode);
    req.playlistId = state.selectedPlaylistId;
    req.limit = 0;
    req.offset = 0;
    req.resetPagination = false;
    req.kind = AsyncSearchKind::CountAndFacetsOnly;
    req.needFacets = needFacets;
    req.isRandomShuffle = false;
    req.deferCount = false;
    req.deferFacets = false;
    req.folderFilterActive = state.folderFilterActive;
    req.selectedFolderPath = state.selectedFolderPath;
    req.hasAdvancedFilters = hasAdvancedFilters;
    req.hasFileTypeFilter = hasFileTypeFilter;
    req.playlistAggregatedView = state.playlistAggregatedView;
    req.requestedAtDbRevision = state.database.dataRevision();

    const std::uint64_t id = state.searchWorker.submitSearch(req);
    state.pendingFacetRequestId = id;
    state.asyncSearchInProgress = true;
    queryTiming.begin(id);
    return id;
}

}  // namespace aura::search


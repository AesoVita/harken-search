#include "AsyncSearchCoverPreloadWorkflow.h"

#include <algorithm>

namespace aura::search {

void HandleAsyncSearchCoverPreload(
    const AsyncSearchCoverPreloadInput& input,
    AsyncSearchCoverPreloadEffects& effects) {
    const bool globalNoFilterBrowse =
        input.databaseView &&
        !input.folderFilterActive &&
        !input.advancedDbFilters &&
        !input.coverFilterActive &&
        !input.findSameFilenameActive &&
        !input.sameFolderScanActive &&
        !input.sameFolderScanDone;
    const bool deferredForCoverWall =
        globalNoFilterBrowse &&
        (input.coverWallPriorityMode ||
         input.coverWallPreloadInFlight ||
         input.coverWallEntriesInFlight ||
         input.coverWallFoldersInFlight ||
         input.coverWallHasPreloadList);
    if (deferredForCoverWall || input.settleFramesRemaining > 0) return;
    const std::size_t count = (std::min)(input.resultCount, std::size_t{30});
    for (std::size_t index = 0; index < count; ++index) effects.preloadResult(index);
}

}  // namespace aura::search

#pragma once

#include <cstddef>

namespace aura::search {

struct AsyncSearchCoverPreloadInput {
    bool databaseView = false;
    bool folderFilterActive = false;
    bool advancedDbFilters = false;
    bool coverFilterActive = false;
    bool findSameFilenameActive = false;
    bool sameFolderScanActive = false;
    bool sameFolderScanDone = false;
    bool coverWallPriorityMode = false;
    bool coverWallPreloadInFlight = false;
    bool coverWallEntriesInFlight = false;
    bool coverWallFoldersInFlight = false;
    bool coverWallHasPreloadList = false;
    int settleFramesRemaining = 0;
    std::size_t resultCount = 0;
};

class AsyncSearchCoverPreloadEffects {
public:
    virtual ~AsyncSearchCoverPreloadEffects() = default;
    virtual void preloadResult(std::size_t index) = 0;
};

void HandleAsyncSearchCoverPreload(
    const AsyncSearchCoverPreloadInput& input,
    AsyncSearchCoverPreloadEffects& effects);

}  // namespace aura::search

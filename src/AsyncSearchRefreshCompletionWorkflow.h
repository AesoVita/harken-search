#pragma once

#include <string>

namespace aura::search {

struct AsyncSearchRefreshCompletionInput {
    bool databaseView = false;
    bool playlistView = false;
    bool playlistLibraryView = false;
    bool folderFilterActive = false;
    bool advancedDbFilters = false;
    bool coverFilterActive = false;
    bool findSameFilenameActive = false;
    bool sameFolderScanActive = false;
    bool sameFolderScanDone = false;
    bool resultsEmpty = false;
    std::string databaseError;
};

class AsyncSearchRefreshCompletionEffects {
public:
    virtual ~AsyncSearchRefreshCompletionEffects() = default;
    virtual void markGenerationApplied() = 0;
    virtual void requestCoverWallRefresh() = 0;
    virtual void setError(const std::string& error) = 0;
    virtual void setReady() = 0;
};

void HandleAsyncSearchRefreshCompletion(
    const AsyncSearchRefreshCompletionInput& input,
    AsyncSearchRefreshCompletionEffects& effects);

}  // namespace aura::search

#include "AsyncSearchRefreshCompletionWorkflow.h"

namespace aura::search {

void HandleAsyncSearchRefreshCompletion(
    const AsyncSearchRefreshCompletionInput& input,
    AsyncSearchRefreshCompletionEffects& effects) {
    effects.markGenerationApplied();
    const bool requestCoverWallRefresh =
        input.playlistView || input.playlistLibraryView ||
        (input.databaseView &&
         (input.folderFilterActive || input.advancedDbFilters ||
          input.coverFilterActive || input.findSameFilenameActive ||
          input.sameFolderScanActive || input.sameFolderScanDone));
    if (requestCoverWallRefresh) effects.requestCoverWallRefresh();
    if (!input.databaseError.empty() && input.databaseView && input.resultsEmpty) {
        effects.setError(input.databaseError);
    } else {
        effects.setReady();
    }
}

}  // namespace aura::search

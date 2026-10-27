#include "AsyncSearchPlaylistResultPreparationWorkflow.h"

namespace aura::search {

AsyncSearchPlaylistResultPreparationOutput
HandleAsyncSearchPlaylistResultPreparation(
    const AsyncSearchPlaylistResultPreparationInput& input,
    AsyncSearchPlaylistResultPreparationEffects& effects) {
    if (input.playlistView) {
        const bool rebuildFolderTree =
            input.refresh ? input.totalCount <= input.loadAllThreshold : input.loadAll;
        if (input.refresh && !rebuildFolderTree) {
            effects.clearPlaylistFolderSearch();
        }
        const int loadedCount = effects.applyPlaylistFilters(rebuildFolderTree);
        if (input.refresh) {
            effects.reportPlaylistRefresh(loadedCount, rebuildFolderTree);
        }
        return {true, loadedCount};
    }
    if (input.libraryView) {
        const int loadedCount = effects.applyLibraryFilters();
        if (input.refresh) {
            effects.reportLibraryRefresh(input.rawCount, loadedCount);
        }
        return {true, loadedCount};
    }
    return {};
}

}  // namespace aura::search

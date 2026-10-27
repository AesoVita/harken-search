#include "AsyncSearchPlaylistContextWorkflow.h"

namespace aura::search {

void HandleAsyncSearchPlaylistContext(
    const AsyncSearchPlaylistContextInput& input,
    AsyncSearchPlaylistContextEffects& effects) {
    switch (input.kind) {
    case AsyncSearchPlaylistContextKind::Playlist:
        effects.applyPlaylistContext(
            input.playlistId,
            input.aggregatedView,
            input.folderFilterActive,
            input.selectedFolderPath);
        effects.reportPlaylistContext(input);
        break;
    case AsyncSearchPlaylistContextKind::Library:
        effects.applyLibraryContext();
        effects.reportLibraryContext(input);
        break;
    case AsyncSearchPlaylistContextKind::None:
        break;
    }
}

}  // namespace aura::search

#pragma once

#include "AppState.h"

void StartDeferredSidebarBootstrap(
    AppState& state,
    bool reloadFolderTree = true,
    bool reloadPlaylists = true);

namespace aura::sidebar_bootstrap {

void Pump(AppState& state);
bool IsFolderTreeLoading();
bool ArePlaylistsLoading();

}  // namespace aura::sidebar_bootstrap

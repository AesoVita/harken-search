#pragma once

#include "Database.h"

#include <cstdint>
#include <vector>

namespace aura::playlist {

// @stage7-contract
// Synchronously resolves the displayed count from caller-owned playlist
// metadata. No database IO is performed and no references are retained.
int64_t ResolvePlaylistDisplayedFileCount(
    const std::vector<Playlist>& playlists,
    int64_t playlistId);

}  // namespace aura::playlist

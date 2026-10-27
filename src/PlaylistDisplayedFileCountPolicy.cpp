#include "PlaylistDisplayedFileCountPolicy.h"

namespace aura::playlist {

int64_t ResolvePlaylistDisplayedFileCount(
    const std::vector<Playlist>& playlists,
    int64_t playlistId) {
    const Playlist* playlist = nullptr;
    for (const auto& candidate : playlists) {
        if (candidate.id == playlistId) {
            playlist = &candidate;
            break;
        }
    }
    if (!playlist) return 0;
    if (!playlist->is_folder) return playlist->fileCount;

    int64_t total = 0;
    for (const auto& child : playlists) {
        if (child.parent_id != playlistId) continue;
        const int64_t childCount =
            ResolvePlaylistDisplayedFileCount(playlists, child.id);
        if (childCount < 0) return -1;
        total += childCount;
    }
    return total;
}

}  // namespace aura::playlist

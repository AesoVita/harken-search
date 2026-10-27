#include "PlaylistDisplayedFileCountPolicy.h"

#include <cassert>
#include <vector>

namespace {

Playlist MakePlaylist(
    int64_t id,
    int64_t parentId,
    bool isFolder,
    int64_t fileCount) {
    Playlist playlist;
    playlist.id = id;
    playlist.parent_id = parentId;
    playlist.is_folder = isFolder;
    playlist.fileCount = fileCount;
    return playlist;
}

}  // namespace

int main() {
    const std::vector<Playlist> playlists{
        MakePlaylist(1, -1, false, 7),
        MakePlaylist(10, -1, true, 0),
        MakePlaylist(11, 10, false, 3),
        MakePlaylist(12, 10, true, 0),
        MakePlaylist(13, 12, false, 5),
    };

    assert(aura::playlist::ResolvePlaylistDisplayedFileCount(playlists, 1) == 7);
    assert(aura::playlist::ResolvePlaylistDisplayedFileCount(playlists, 10) == 8);
    assert(aura::playlist::ResolvePlaylistDisplayedFileCount(playlists, 999) == 0);

    auto pending = playlists;
    pending.push_back(MakePlaylist(14, 12, false, -1));
    assert(aura::playlist::ResolvePlaylistDisplayedFileCount(pending, 10) == -1);
    return 0;
}

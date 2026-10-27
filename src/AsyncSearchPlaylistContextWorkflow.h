#pragma once

#include <cstdint>
#include <string>

namespace aura::search {

enum class AsyncSearchPlaylistContextKind {
    None,
    Playlist,
    Library,
};

struct AsyncSearchPlaylistContextInput {
    AsyncSearchPlaylistContextKind kind = AsyncSearchPlaylistContextKind::None;
    std::uint64_t requestId = 0;
    std::int64_t playlistId = -1;
    bool aggregatedView = false;
    bool folderFilterActive = false;
    std::string selectedFolderPath;
    int resultKind = 0;
};

class AsyncSearchPlaylistContextEffects {
public:
    virtual ~AsyncSearchPlaylistContextEffects() = default;
    virtual void applyPlaylistContext(
        std::int64_t playlistId,
        bool aggregatedView,
        bool folderFilterActive,
        const std::string& selectedFolderPath) = 0;
    virtual void reportPlaylistContext(const AsyncSearchPlaylistContextInput& input) = 0;
    virtual void applyLibraryContext() = 0;
    virtual void reportLibraryContext(const AsyncSearchPlaylistContextInput& input) = 0;
};

void HandleAsyncSearchPlaylistContext(
    const AsyncSearchPlaylistContextInput& input,
    AsyncSearchPlaylistContextEffects& effects);

}  // namespace aura::search

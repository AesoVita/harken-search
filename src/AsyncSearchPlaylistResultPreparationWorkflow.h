#pragma once

#include <cstddef>
#include <cstdint>

namespace aura::search {

struct AsyncSearchPlaylistResultPreparationInput {
    bool playlistView = false;
    bool libraryView = false;
    bool refresh = false;
    bool loadAll = false;
    std::int64_t totalCount = 0;
    int loadAllThreshold = 0;
    std::size_t rawCount = 0;
};

struct AsyncSearchPlaylistResultPreparationOutput {
    bool prepared = false;
    int loadedCount = 0;
};

class AsyncSearchPlaylistResultPreparationEffects {
public:
    virtual ~AsyncSearchPlaylistResultPreparationEffects() = default;
    virtual void clearPlaylistFolderSearch() = 0;
    virtual int applyPlaylistFilters(bool rebuildFolderTree) = 0;
    virtual int applyLibraryFilters() = 0;
    virtual void reportPlaylistRefresh(
        int loadedCount,
        bool rebuildFolderTree) = 0;
    virtual void reportLibraryRefresh(
        std::size_t rawCount,
        int loadedCount) = 0;
};

AsyncSearchPlaylistResultPreparationOutput
HandleAsyncSearchPlaylistResultPreparation(
    const AsyncSearchPlaylistResultPreparationInput& input,
    AsyncSearchPlaylistResultPreparationEffects& effects);

}  // namespace aura::search

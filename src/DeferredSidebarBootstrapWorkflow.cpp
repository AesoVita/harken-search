#include "DeferredSidebarBootstrapWorkflow.h"

#include "Database.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

struct DeferredSidebarBootstrapState {
    std::mutex mutex;
    std::atomic<uint64_t> generation{0};
    bool active = false;
    bool folderTreePending = false;
    bool folderTreeReady = false;
    bool playlistsPending = false;
    bool playlistsReady = false;
    std::string dbPath;
    FolderInfo folderTreeResult;
    std::vector<Playlist> playlistsResult;
};

DeferredSidebarBootstrapState g_state;

}  // namespace

void StartDeferredSidebarBootstrap(
    AppState& state,
    bool reloadFolderTree,
    bool reloadPlaylists) {
    if (!state.database.isOpen()) return;
    if (!reloadFolderTree && !reloadPlaylists) return;

    const uint64_t generation =
        g_state.generation.fetch_add(1, std::memory_order_acq_rel) + 1;
    const std::string dbPath = state.currentDbPathUtf8;
    const aura::folder_tree::PathMode folderTreePathMode = state.settings.folderTreePathMode;

    {
        std::lock_guard<std::mutex> lock(g_state.mutex);
        g_state.active = true;
        g_state.dbPath = dbPath;
        if (reloadFolderTree) {
            g_state.folderTreePending = true;
            g_state.folderTreeReady = false;
            g_state.folderTreeResult = FolderInfo{};
        }
        if (reloadPlaylists) {
            g_state.playlistsPending = true;
            g_state.playlistsReady = false;
            g_state.playlistsResult.clear();
        }
    }

    if (reloadFolderTree) {
        std::thread([dbPath, generation, folderTreePathMode]() {
            FolderInfo tree;
            Database db;
            if (db.openReadOnly(dbPath)) {
                tree = db.getFolderHierarchyFromIndex(folderTreePathMode);
                if (tree.subfolders.empty()) {
                    tree = db.getFolderHierarchy(folderTreePathMode);
                }
            }

            std::lock_guard<std::mutex> lock(g_state.mutex);
            if (g_state.generation.load(std::memory_order_acquire) != generation ||
                g_state.dbPath != dbPath) {
                return;
            }
            g_state.folderTreeResult = std::move(tree);
            g_state.folderTreeReady = true;
            g_state.folderTreePending = false;
        }).detach();
    }

    if (reloadPlaylists) {
        std::thread([dbPath, generation]() {
            auto publishPlaylists =
                [dbPath, generation](std::vector<Playlist>&& playlists,
                                     bool stillPending) {
                    std::lock_guard<std::mutex> lock(g_state.mutex);
                    if (g_state.generation.load(std::memory_order_acquire) != generation ||
                        g_state.dbPath != dbPath) {
                        return false;
                    }
                    g_state.playlistsResult = std::move(playlists);
                    g_state.playlistsReady = true;
                    g_state.playlistsPending = stillPending;
                    return true;
                };

            Database db;
            if (db.openReadOnly(dbPath)) {
                std::vector<Playlist> metadata = db.getAllPlaylists(false);
                if (!publishPlaylists(std::move(metadata), true)) return;
                std::vector<Playlist> playlistsWithCounts = db.getAllPlaylists(true);
                publishPlaylists(std::move(playlistsWithCounts), false);
                return;
            }

            std::lock_guard<std::mutex> lock(g_state.mutex);
            if (g_state.generation.load(std::memory_order_acquire) != generation ||
                g_state.dbPath != dbPath) {
                return;
            }
            g_state.playlistsResult.clear();
            g_state.playlistsReady = true;
            g_state.playlistsPending = false;
        }).detach();
    }
}

namespace aura::sidebar_bootstrap {

void Pump(AppState& state) {
    FolderInfo folderTree;
    std::vector<Playlist> playlists;
    bool hasFolderTree = false;
    bool hasPlaylists = false;

    {
        std::lock_guard<std::mutex> lock(g_state.mutex);
        if (g_state.folderTreeReady) {
            folderTree = std::move(g_state.folderTreeResult);
            g_state.folderTreeResult = FolderInfo{};
            g_state.folderTreeReady = false;
            hasFolderTree = true;
        }
        if (g_state.playlistsReady) {
            playlists = std::move(g_state.playlistsResult);
            g_state.playlistsResult.clear();
            g_state.playlistsReady = false;
            hasPlaylists = true;
        }
        g_state.active =
            g_state.folderTreePending || g_state.playlistsPending ||
            g_state.folderTreeReady || g_state.playlistsReady;
    }

    if (hasFolderTree) {
        state.setFolderHierarchy(std::move(folderTree), state.currentDbPathUtf8);
        state.folderSearchHits.clear();
        state.folderSearchDirectHits.clear();
        state.folderSearchExpandPaths.clear();
        state.lastFolderSearch.clear();
    }
    if (hasPlaylists) state.playlists = std::move(playlists);
}

bool IsFolderTreeLoading() {
    std::lock_guard<std::mutex> lock(g_state.mutex);
    return g_state.folderTreePending;
}

bool ArePlaylistsLoading() {
    std::lock_guard<std::mutex> lock(g_state.mutex);
    return g_state.playlistsPending;
}

}  // namespace aura::sidebar_bootstrap

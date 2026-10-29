#pragma once

#include "PathUtils.h"
#include "FolderTreePathModePolicy.h"

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace aura::search {

// Keep the drive and library parent open, with category folders collapsed.
inline bool ShouldOpenSearchFolderInitially(const std::string& path) {
    const auto layers = aura::folder_tree::BuildFolderTreeLayers(
        path, {}, aura::folder_tree::PathMode::Absolute);
    return !layers.empty() && layers.size() <= 2;
}

// Immutable search snapshot, shared with tabs. Build once on the search worker;
// rendering only checks membership and never queries SQLite or the filesystem.
struct SearchResultFolderTreeSnapshot {
    std::string query;
    std::unordered_set<std::string> visiblePaths;

    bool active() const {
        return query.find_first_not_of(" \t\r\n") != std::string::npos;
    }

    bool preserveForFolderNavigation(bool directoryNavigation, const std::string& keyword) const {
        return directoryNavigation && active() && query == keyword;
    }

    bool contains(const std::string& path) const {
        return !active() || visiblePaths.count(HarkenSearch::CanonicalFolderPath(path)) != 0;
    }
};

inline std::shared_ptr<const SearchResultFolderTreeSnapshot>
BuildSearchResultFolderTreeSnapshot(const std::string& query,
                                   const std::vector<std::string>& matchingFolders) {
    auto snapshot = std::make_shared<SearchResultFolderTreeSnapshot>();
    snapshot->query = query;
    if (!snapshot->active()) return snapshot;
    for (const auto& folder : matchingFolders) {
        const std::string path = HarkenSearch::CanonicalFolderPath(folder);
        if (path.empty()) continue;
        snapshot->visiblePaths.insert(path);
        // Use the same layer identities as the actual sidebar tree, including
        // its drive node ("c:") and UNC ancestors. Also accept drive-root paths.
        for (const auto& layer : aura::folder_tree::BuildFolderTreeLayers(
                 path, {}, aura::folder_tree::PathMode::Absolute)) {
            snapshot->visiblePaths.insert(HarkenSearch::CanonicalFolderPath(layer.path));
            if (layer.path.size() == 2 && layer.path[1] == ':') {
                snapshot->visiblePaths.insert(HarkenSearch::CanonicalFolderPath(layer.path + "\\"));
            }
        }
    }
    return snapshot;
}

}  // namespace aura::search

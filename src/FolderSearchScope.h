#pragma once
#include "PathUtils.h"
#include "FolderTreePathModePolicy.h"
#include <string>

namespace aura::search {
// The selected result directory is navigation, while this explicit lock is
// the only folder boundary carried into subsequent keyword searches.
struct FolderSearchScope {
    std::string lockedPath;

    bool locked() const { return !lockedPath.empty(); }
    bool lock(const std::string& selectedFolder, bool searchEmpty) {
        if (!searchEmpty || selectedFolder.find_first_not_of(" \t\r\n") == std::string::npos) return false;
        lockedPath = HarkenSearch::CanonicalFolderPath(selectedFolder);
        return locked();
    }
    void unlock() { lockedPath.clear(); }
    std::string folderForNewSearch() const { return lockedPath; }
    std::string folderForResultView(const std::string& folder) const {
        const auto selected = HarkenSearch::CanonicalFolderPath(folder);
        if (!locked()) return selected;
        return aura::folder_tree::PathIsInsideRoot(selected, lockedPath) ? selected : lockedPath;
    }
};
} // namespace aura::search

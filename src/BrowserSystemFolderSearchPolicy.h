#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace aura::browser {

struct SystemFolderSearchHit {
    std::string path;
    std::string displayName;
    std::string parentFolder;
    bool isFile = false;
};

struct SystemFolderSearchTreeNode {
    std::string path;
    std::string displayName;
    std::string parentFolder;
    bool directHit = false;
    bool isFile = false;
    std::map<std::string, SystemFolderSearchTreeNode> children;
};

// Inputs are borrowed for the duration of each call; returned containers own
// their strings and projected tree nodes.
bool MatchesSystemFolderSearchQuery(
    const std::string& displayName,
    const std::string& folderPath,
    const std::vector<std::string>& terms);

bool IsSystemFolderAncestorOf(
    const std::string& folderPath,
    const std::string& targetPath);

std::vector<std::pair<std::string, std::string>>
BuildSystemFolderPathChain(const std::string& folderPath);

std::map<std::string, SystemFolderSearchTreeNode>
BuildSystemFolderSearchTree(const std::vector<SystemFolderSearchHit>& hits);

} // namespace aura::browser

#include "BrowserSystemFolderSearchPolicy.h"

#include <algorithm>
#include <cctype>

namespace aura::browser {

bool MatchesSystemFolderSearchQuery(
    const std::string& displayName,
    const std::string& folderPath,
    const std::vector<std::string>& terms) {
    if (terms.empty()) {
        return true;
    }

    std::string nameLower = displayName;
    std::string pathLower = folderPath;
    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
    std::transform(pathLower.begin(), pathLower.end(), pathLower.begin(), ::tolower);

    for (const auto& term : terms) {
        if (nameLower.find(term) == std::string::npos &&
            pathLower.find(term) == std::string::npos) {
            return false;
        }
    }
    return true;
}

bool IsSystemFolderAncestorOf(
    const std::string& folderPath,
    const std::string& targetPath) {
    if (folderPath.empty() || targetPath.empty()) return false;
    if (targetPath.size() <= folderPath.size()) return false;
    if (targetPath.compare(0, folderPath.size(), folderPath) == 0) {
        const char next = targetPath[folderPath.size()];
        return next == '/' || next == '\\';
    }
    return false;
}

std::vector<std::pair<std::string, std::string>>
BuildSystemFolderPathChain(const std::string& folderPath) {
    std::vector<std::pair<std::string, std::string>> chain;
    std::string normalized = folderPath;
    std::replace(normalized.begin(), normalized.end(), '/', '\\');
    while (normalized.size() > 3 &&
           (normalized.back() == '\\' || normalized.back() == '/')) {
        normalized.pop_back();
    }

#ifdef _WIN32
    if (normalized.size() >= 3 &&
        std::isalpha(static_cast<unsigned char>(normalized[0])) &&
        normalized[1] == ':' && normalized[2] == '\\') {
        std::string current;
        current.push_back(static_cast<char>(
            std::toupper(static_cast<unsigned char>(normalized[0]))));
        current += ":\\";
        chain.push_back({current, current});
        size_t start = 3;
        while (start < normalized.size()) {
            const size_t end = normalized.find('\\', start);
            const std::string part = normalized.substr(
                start,
                end == std::string::npos ? std::string::npos : end - start);
            if (!part.empty()) {
                if (current.size() > 3 && current.back() != '\\') {
                    current += "\\";
                }
                current += part;
                chain.push_back({current, part});
            }
            if (end == std::string::npos) break;
            start = end + 1;
        }
        return chain;
    }
#endif

    std::string current;
    size_t start = 0;
    while (start < normalized.size()) {
        const size_t end = normalized.find('\\', start);
        const std::string part = normalized.substr(
            start,
            end == std::string::npos ? std::string::npos : end - start);
        if (!part.empty()) {
            if (!current.empty()) current += "\\";
            current += part;
            chain.push_back({current, part});
        }
        if (end == std::string::npos) break;
        start = end + 1;
    }
    if (chain.empty() && !folderPath.empty()) {
        chain.push_back({folderPath, folderPath});
    }
    return chain;
}

std::map<std::string, SystemFolderSearchTreeNode>
BuildSystemFolderSearchTree(const std::vector<SystemFolderSearchHit>& hits) {
    std::map<std::string, SystemFolderSearchTreeNode> roots;
    for (const auto& hit : hits) {
        const auto chain = BuildSystemFolderPathChain(hit.path);
        if (chain.empty()) continue;
        auto* level = &roots;
        for (size_t i = 0; i < chain.size(); ++i) {
            const auto& part = chain[i];
            auto [it, inserted] = level->try_emplace(part.first);
            auto& node = it->second;
            if (inserted || node.path.empty()) {
                node.path = part.first;
                node.displayName = part.second.empty() ? part.first : part.second;
                node.parentFolder = part.first;
            }
            if (i + 1 == chain.size()) {
                node.directHit = true;
                if (!hit.displayName.empty()) {
                    node.displayName = hit.displayName;
                }
            }
            level = &node.children;
        }
    }
    return roots;
}

} // namespace aura::browser

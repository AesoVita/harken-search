#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace aura::tag_cloud {

struct TagCloudContextIdentity final {
    std::string databaseIdentity;
    std::string databasePath;
    std::string tabId;
    std::uint64_t searchResultsGeneration = 0;
    std::uint64_t filterQueryGeneration = 0;

    bool operator==(const TagCloudContextIdentity& other) const noexcept {
        return databaseIdentity == other.databaseIdentity &&
               databasePath == other.databasePath &&
               tabId == other.tabId &&
               searchResultsGeneration == other.searchResultsGeneration &&
               filterQueryGeneration == other.filterQueryGeneration;
    }
};

struct TagCloudResult final {
    TagCloudContextIdentity identity;
    std::vector<std::pair<std::string, int>> tags;
};

inline bool IsCurrent(const TagCloudResult& result,
                      const TagCloudContextIdentity& current) noexcept {
    return result.identity == current;
}

}  // namespace aura::tag_cloud

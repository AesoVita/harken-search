#pragma once

#include "CoverModule.h"
#include "Database.h"

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace aura::playlist {

struct PlaylistCoverGroup {
    std::string key;
    int64_t hitCount = 0;
};

inline void AddPlaylistCoverKey(std::vector<PlaylistCoverGroup>& groups,
                                std::unordered_map<std::string, size_t>& index,
                                const std::string& key) {
    if (key.empty()) return;
    const auto [position, inserted] = index.emplace(key, groups.size());
    if (inserted) {
        PlaylistCoverGroup group;
        group.key = key;
        group.hitCount = 1;
        groups.push_back(std::move(group));
    } else {
        groups[position->second].hitCount++;
    }
}

inline std::vector<PlaylistCoverGroup> BuildPlaylistCoverGroupsFromRecords(
    const std::vector<AudioRecord>& records) {
    std::vector<PlaylistCoverGroup> groups;
    std::unordered_map<std::string, size_t> index;
    index.reserve(records.size());
    for (const AudioRecord& record : records) {
        AddPlaylistCoverKey(groups, index, record.folderCoverKey);
        AddPlaylistCoverKey(groups, index, record.embeddedCoverKey);
    }
    return groups;
}

inline std::vector<PlaylistCoverGroup> BuildPlaylistCoverGroupsFromCompleteMembers(
    const std::vector<AudioRecord>& completeMembers,
    const std::vector<AudioRecord>& loadedPage) {
    (void)loadedPage;
    return BuildPlaylistCoverGroupsFromRecords(completeMembers);
}

inline std::vector<CoverModule::CoverItem> BuildPlaylistCoverItemsFromGroups(
    const std::vector<PlaylistCoverGroup>& groups) {
    std::vector<CoverModule::CoverItem> items;
    items.reserve(groups.size());
    for (const PlaylistCoverGroup& group : groups) {
        if (group.key.empty() || group.hitCount <= 0) continue;
        CoverModule::CoverItem item;
        item.key = group.key;
        item.hitCount = group.hitCount;
        if (group.key.rfind("E|", 0) == 0) {
            item.kind = CoverModule::CoverKind::EmbeddedCover;
        } else {
            item.kind = CoverModule::CoverKind::FolderCover;
        }
        items.push_back(std::move(item));
    }
    return items;
}

}  // namespace aura::playlist

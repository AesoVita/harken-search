#include "CoverCachedKeyItemBuilder.h"

#include "PathUtils.h"

#include <algorithm>
#include <string>
#include <unordered_map>

namespace aura::cover_wall {

namespace {

std::string BaseNameFromUtf8Path(const std::string& path) {
    if (path.empty()) return {};
    const size_t separator = path.find_last_of("/\\");
    return separator == std::string::npos ? path : path.substr(separator + 1);
}

}  // namespace

bool IsRealCoverWallItemCandidate(const CoverModule::CoverItem& item) {
    if (item.key.empty() || item.isPlaceholder) return false;
    if (item.kind == CoverModule::CoverKind::EmbeddedCover) {
        return item.key.rfind("E|", 0) == 0 && !item.filePathUtf8.empty();
    }
    if (item.key.rfind("F|", 0) == 0) {
        return !item.folderPathUtf8.empty() || !item.folderCoverSrcPathUtf8.empty();
    }
    if (item.key.rfind("?|", 0) == 0) {
        return !item.folderPathUtf8.empty();
    }
    return false;
}

void SanitizeCoverWallItemsForCache(std::vector<CoverModule::CoverItem>& items) {
    if (items.empty()) return;
    items.erase(
        std::remove_if(
            items.begin(), items.end(),
            [](const CoverModule::CoverItem& item) {
                return !IsRealCoverWallItemCandidate(item) ||
                    item.key == "\1" || item.key == "E|\1" || item.key == "F|\1";
            }),
        items.end());
}

void BuildCoverWallItemsFromCachedKeys(
    const std::vector<AudioRecord>& records,
    std::vector<CoverModule::CoverItem>& outItems) {
    outItems.clear();
    if (records.size() > kMaxRecordsForCoverBuildOnUIThread) return;

    std::unordered_map<std::string, size_t> keyToIndex;
    keyToIndex.reserve((std::min)(records.size(), size_t(65536)));

    for (const auto& record : records) {
        std::string folderKey = record.folderCoverKey;
        std::string folderCoverPath = record.folderCoverSrcPath;
        if (folderKey.empty() && !record.folderPath.empty()) {
            CoverModule::TryGetFolderCoverKeyFast(
                record.folderPath, folderKey, &folderCoverPath);
        }
        if (!folderKey.empty()) {
            folderKey = CoverModule::CanonicalCoverKey(folderKey);
        }

        const bool allowTemporaryFolderCover =
            folderKey.empty() &&
            record.embeddedCoverKey.empty() &&
            record.hasCover &&
            !record.folderPath.empty();
        const std::string temporaryFolderKey = allowTemporaryFolderCover
            ? std::string("?|") + HarkenSearch::CanonicalFolderPath(record.folderPath)
            : std::string();

        if (!folderKey.empty() || !temporaryFolderKey.empty()) {
            const std::string& wallKey = !folderKey.empty() ? folderKey : temporaryFolderKey;
            auto [position, inserted] = keyToIndex.emplace(wallKey, outItems.size());
            if (inserted) {
                CoverModule::CoverItem item;
                item.kind = CoverModule::CoverKind::FolderCover;
                item.key = wallKey;
                item.folderPathUtf8 = record.folderPath;
                item.folderCoverSrcPathUtf8 = folderCoverPath;
                item.hitCount = 1;
                item.label = BaseNameFromUtf8Path(record.folderPath);
                outItems.push_back(std::move(item));
            } else {
                outItems[position->second].hitCount++;
            }
        }

        if (!record.embeddedCoverKey.empty()) {
            std::string embeddedKey = CoverModule::CanonicalCoverKey(record.embeddedCoverKey);
            if (!embeddedKey.empty()) {
                auto [position, inserted] = keyToIndex.emplace(embeddedKey, outItems.size());
                if (inserted) {
                    CoverModule::CoverItem item;
                    item.kind = CoverModule::CoverKind::EmbeddedCover;
                    item.key = embeddedKey;
                    item.filePathUtf8 = record.filePath;
                    item.hitCount = 1;
                    item.label = BaseNameFromUtf8Path(record.filePath);
                    outItems.push_back(std::move(item));
                } else {
                    outItems[position->second].hitCount++;
                }
            }
        }
    }

    SanitizeCoverWallItemsForCache(outItems);
}

void BuildCoverWallItemsFromSearchResultsFallback(
    const std::vector<AudioRecord>& searchResults,
    std::vector<CoverModule::CoverItem>& outItems) {
    BuildCoverWallItemsFromCachedKeys(searchResults, outItems);
}

}  // namespace aura::cover_wall

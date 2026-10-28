#include "CoverWallPageResultBuilder.h"

#include "CoverCachedKeyItemBuilder.h"
#include "PathUtils.h"

#include <unordered_set>

namespace aura::cover_wall {

namespace {

std::string BaseNameFromUtf8Path(const std::string& path) {
    if (path.empty()) return {};
    const size_t separator = path.find_last_of("/\\");
    return separator == std::string::npos ? path : path.substr(separator + 1);
}

}  // namespace

CoverWallPageLoadResult LoadGlobalCoverWallPage(const std::string& dbPath,
                                                const std::string& afterCursor,
                                                int limit) {
    Database localDb;
    if (!localDb.openReadOnly(dbPath)) {
        return {};
    }
    auto page = localDb.getGlobalCoverWallEntriesPage(afterCursor, limit, true);
    // Old libraries can mark a folder as covered without storing the artwork
    // key. Several child folders may then display the very same parent image.
    // Resolve those placeholders on this background worker before deduplication.
    for (auto& entry : page.entries) {
        if (entry.isEmbedded || entry.key.rfind("?|", 0) != 0 || !entry.sourcePath.empty()) continue;
        std::string coverPath;
        if (CoverModule::GetFolderCoverImagePathBackground(entry.path, coverPath, true, true)) {
            entry.sourcePath = std::move(coverPath);
        }
    }
    return BuildCoverWallPageLoadResult(page);
}

CoverWallPageLoadResult BuildCoverWallPageFromCurrentFileList(
    const std::vector<AudioRecord>& records) {
    CoverWallPageLoadResult result;
    if (records.empty()) return result;
    BuildCoverWallItemsFromCachedKeys(records, result.items);
    result.hasMore = false;
    result.fromRegistry = false;
    result.largeLibrary = false;
    return result;
}

CoverModule::CoverItem ConvertCoverWallEntryToItem(const Database::CoverWallEntry& entry) {
    CoverModule::CoverItem item;
    item.key = entry.key;
    item.kind = entry.isEmbedded ? CoverModule::CoverKind::EmbeddedCover
                                 : CoverModule::CoverKind::FolderCover;
    if (entry.isEmbedded) {
        item.filePathUtf8 = entry.path;
        if (entry.sourcePath.rfind("PHASH|", 0) == 0) {
            item.folderCoverSrcPathUtf8 = entry.sourcePath;
        }
        item.label = BaseNameFromUtf8Path(item.filePathUtf8);
    } else {
        item.folderPathUtf8 = entry.path;
        item.folderCoverSrcPathUtf8 = entry.sourcePath;
        item.label = BaseNameFromUtf8Path(item.folderPathUtf8);
    }
    return item;
}

std::string BuildCoverWallVisualDedupeToken(const CoverModule::CoverItem& item) {
    if (item.kind == CoverModule::CoverKind::EmbeddedCover &&
        item.folderCoverSrcPathUtf8.rfind("PHASH|", 0) == 0) {
        return item.folderCoverSrcPathUtf8;
    }
    if (item.kind == CoverModule::CoverKind::FolderCover &&
        !item.folderCoverSrcPathUtf8.empty() &&
        item.folderCoverSrcPathUtf8.rfind("PHASH|", 0) != 0) {
        return "F|SRC|" + HarkenSearch::CanonicalFolderPath(item.folderCoverSrcPathUtf8);
    }
    return item.key;
}

void AppendUniqueCoverWallPageItems(std::vector<CoverModule::CoverItem>& existing,
                                    std::vector<CoverModule::CoverItem>& nextPage) {
    std::unordered_set<std::string> seenTokens;
    std::unordered_set<std::string> seenKeys;
    seenTokens.reserve(existing.size() + nextPage.size());
    seenKeys.reserve(existing.size() + nextPage.size());
    for (const auto& item : existing) {
        seenTokens.insert(BuildCoverWallVisualDedupeToken(item));
        seenKeys.insert(item.key);
    }
    for (auto& item : nextPage) {
        const std::string token = BuildCoverWallVisualDedupeToken(item);
        if (item.key.empty() || token.empty()) continue;
        const bool newKey = seenKeys.insert(item.key).second;
        const bool newToken = seenTokens.insert(token).second;
        if (newKey && newToken) {
            existing.push_back(std::move(item));
        }
    }
}

CoverWallPageLoadResult BuildCoverWallPageLoadResult(const Database::CoverWallPage& page) {
    CoverWallPageLoadResult result;
    result.nextCursor = page.nextCursor;
    result.hasMore = page.hasMore;
    result.fromRegistry = page.fromRegistry;
    result.largeLibrary = page.largeLibrary;
    result.registryEntries = page.registryEntries;
    result.items.reserve(page.entries.size());
    std::vector<CoverModule::CoverItem> converted;
    converted.reserve(page.entries.size());
    for (const auto& entry : page.entries) {
        if (entry.key.empty() || entry.path.empty()) continue;
        converted.push_back(ConvertCoverWallEntryToItem(entry));
    }
    AppendUniqueCoverWallPageItems(result.items, converted);
    SanitizeCoverWallItemsForCache(result.items);
    return result;
}

CoverWallPageLoadResult BuildCoverWallLoadResultFromEntries(
    const std::vector<Database::CoverWallEntry>& entries) {
    Database::CoverWallPage page;
    page.entries = entries;
    return BuildCoverWallPageLoadResult(page);
}

}  // namespace aura::cover_wall

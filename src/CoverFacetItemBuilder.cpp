#include "CoverFacetItemBuilder.h"

#include "Utils.h"

#include <string>
#include <unordered_set>

namespace aura::cover_wall {

namespace {

std::string BaseNameFromUtf8Path(const std::string& path) {
    if (path.empty()) return {};
    const size_t separator = path.find_last_of("/\\");
    return separator == std::string::npos ? path : path.substr(separator + 1);
}

}  // namespace

void BuildCoverWallItemsFromCoverFacets(
    const std::vector<Database::CoverFacetEntry>& facets,
    std::vector<CoverModule::CoverItem>& outItems) {
    outItems.clear();
    if (facets.empty()) return;

    outItems.reserve(facets.size());
    std::unordered_set<std::string> seenFacetKeys;
    seenFacetKeys.reserve(facets.size());

    for (const auto& facet : facets) {
        CoverModule::CoverItem item;
        std::string normalizedKey = facet.key;
        if (!normalizedKey.empty() && normalizedKey.rfind("?|", 0) != 0) {
            normalizedKey = CoverModule::CanonicalCoverKey(normalizedKey);
            if (normalizedKey.empty()) continue;
        }
        if (!seenFacetKeys.insert(normalizedKey).second) continue;

        item.key = normalizedKey;
        item.hitCount = facet.count;
        if (facet.key.rfind("?|", 0) == 0) {
            item.kind = CoverModule::CoverKind::FolderCover;
            item.folderPathUtf8 = facet.path;
            item.label = BaseNameFromUtf8Path(facet.path);
        } else {
            item.kind = facet.isEmbedded
                ? CoverModule::CoverKind::EmbeddedCover
                : CoverModule::CoverKind::FolderCover;
            if (facet.isEmbedded) {
                item.filePathUtf8 = facet.path;
                if (facet.sourcePath.rfind("PHASH|", 0) == 0) {
                    item.folderCoverSrcPathUtf8 = facet.sourcePath;
                }
            } else {
                item.folderPathUtf8 = facet.path;
                item.folderCoverSrcPathUtf8 = facet.sourcePath;
            }
            item.label = BaseNameFromUtf8Path(facet.path);
        }
        outItems.push_back(std::move(item));
    }
}

}  // namespace aura::cover_wall

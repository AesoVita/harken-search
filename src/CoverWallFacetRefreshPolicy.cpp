#include "CoverWallFacetRefreshPolicy.h"

#include "CoverWallPageResultBuilder.h"

namespace aura::cover_wall {

bool CoverWallItemsShareVisualOrder(
    const std::vector<CoverModule::CoverItem>& existing,
    const std::vector<CoverModule::CoverItem>& updated) {
    if (existing.size() != updated.size()) return false;
    for (size_t i = 0; i < existing.size(); ++i) {
        if (BuildCoverWallVisualDedupeToken(existing[i]) !=
            BuildCoverWallVisualDedupeToken(updated[i])) {
            return false;
        }
    }
    return true;
}

void RefreshCoverWallFacetMetadataInPlace(
    std::vector<CoverModule::CoverItem>& existing,
    const std::vector<CoverModule::CoverItem>& updated) {
    if (!CoverWallItemsShareVisualOrder(existing, updated)) {
        existing = updated;
        return;
    }
    for (size_t i = 0; i < existing.size(); ++i) {
        const auto& src = updated[i];
        auto& dst = existing[i];
        dst.hitCount = src.hitCount;
        dst.label = src.label;
        dst.kind = src.kind;
        dst.filePathUtf8 = src.filePathUtf8;
        dst.folderPathUtf8 = src.folderPathUtf8;
        dst.folderCoverSrcPathUtf8 = src.folderCoverSrcPathUtf8;
    }
}

}  // namespace aura::cover_wall

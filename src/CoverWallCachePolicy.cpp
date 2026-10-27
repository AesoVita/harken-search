#include "CoverWallCachePolicy.h"

#include "CoverCachedKeyItemBuilder.h"

namespace aura::cover_wall {

bool CanReuseGlobalCoverWallCache(const AppState&,
                                  const CoverWallState& cw,
                                  const std::string& dbPath,
                                  std::int64_t totalFileCount) {
    return cw.globalItemsCacheValid &&
           cw.globalItemsCacheDbPath == dbPath &&
           cw.globalItemsCacheTotalFileCount == totalFileCount &&
           cw.globalItemsCacheGeneration == cw.generation;
}

void StoreGlobalCoverWallCache(CoverWallState& cw,
                               const std::string& dbPath,
                               std::int64_t totalFileCount) {
    if (cw.globalPageHasMore || cw.globalPageLoadingNext || cw.entriesInFlight) {
        cw.globalItemsCacheValid = false;
        return;
    }
    SanitizeCoverWallItemsForCache(cw.items);
    cw.globalItemsCache = cw.items;
    cw.globalItemsCacheValid = true;
    cw.globalItemsCacheDbPath = dbPath;
    cw.globalItemsCacheTotalFileCount = totalFileCount;
    cw.globalItemsCacheGeneration = cw.generation;
}

std::string LastCoverWallPageCursor(
    const std::vector<CoverModule::CoverItem>& items) {
    for (auto it = items.rbegin(); it != items.rend(); ++it) {
        if (!it->key.empty()) return it->key;
    }
    return {};
}

void ReconstructGlobalCoverWallPageState(CoverWallState& cw,
                                         const std::string& dbPath,
                                         std::size_t pageSize) {
    if (!cw.globalPageDbPath.empty() && cw.globalPageDbPath == dbPath) return;
    cw.globalPageDbPath = dbPath;
    cw.globalPageNextCursor = LastCoverWallPageCursor(cw.items);
    cw.globalPageHasMore = cw.items.size() >= pageSize;
    cw.globalPageLoadingNext = false;
}

}  // namespace aura::cover_wall

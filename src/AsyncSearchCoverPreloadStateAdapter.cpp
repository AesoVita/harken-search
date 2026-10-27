#include "AsyncSearchCoverPreloadStateAdapter.h"

#include "AppState.h"
#include "AsyncSearchCoverPreloadAdapter.h"
#include "CoverModule.h"

#include <utility>

namespace aura::search {

AsyncSearchCoverPreloadStateAdapter::AsyncSearchCoverPreloadStateAdapter(
    AppState& state,
    AdvancedFilterPredicate hasAdvancedFilters,
    CoverWallPriorityPredicate isCoverWallPriorityMode)
    : state_(state),
      hasAdvancedFilters_(std::move(hasAdvancedFilters)),
      isCoverWallPriorityMode_(std::move(isCoverWallPriorityMode)) {}

void AsyncSearchCoverPreloadStateAdapter::apply() {
    AsyncSearchCoverPreloadAdapter coverPreloadEffects(
        [this](std::size_t index,
               AsyncSearchCoverPreloadRecord& record) {
            if (index >= state_.searchResults.size()) return false;
            const auto& source = state_.searchResults[index];
            record = {source.folderPath, source.embeddedCoverKey, source.filePath};
            return true;
        },
        [](const std::string& folderPath) {
            CoverModule::PreCacheFolderCovers({folderPath});
        },
        [](const std::string& coverKey, const std::string& filePath) {
            CoverModule::TextureHandle dummy;
            CoverModule::CoverItem probe;
            probe.kind = CoverModule::CoverKind::EmbeddedCover;
            probe.key = coverKey;
            probe.filePathUtf8 = filePath;
            if (!CoverModule::IsCoverKeyPendingOrFailed(probe)) {
                (void)CoverModule::GetEmbeddedCoverTexture(coverKey, filePath, dummy);
            }
        });

    HandleAsyncSearchCoverPreload(
        {state_.viewMode == ViewMode::Database,
         state_.folderFilterActive,
         hasAdvancedFilters_(state_),
         state_.coverFilterActive,
         state_.findSameFilenameActive,
         state_.sameFolderScanActive,
         state_.sameFolderScanDone,
         isCoverWallPriorityMode_(),
         state_.coverWall.preloadInFlight,
         state_.coverWall.entriesInFlight,
         state_.coverWall.foldersInFlight,
         !state_.coverWall.preloadFullList.empty(),
         state_.dbSwitchSettleFramesRemaining,
         state_.searchResults.size()},
        coverPreloadEffects);
}

}  // namespace aura::search

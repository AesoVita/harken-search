#include "AsyncSearchBrowsePreviewWorkflow.h"

namespace aura::search {

aura::coverwall::BrowseSurfacePreviewSelection CaptureAsyncSearchBrowsePreview(
    const AsyncSearchBrowsePreviewCaptureInput& input) {
    aura::coverwall::BrowseSurfacePreviewSelection selection;
    if (input.hasSelectedRecord) {
        selection.preservedFileId = input.selectedFileId;
        selection.preservedFilePath = input.selectedFilePath;
    }
    if (input.integratedMode && input.coverFilterActive &&
        input.pinnedCoverKey == input.coverFilterKey) {
        selection.preferredFilePath = input.pinnedFilePath;
    }
    if (input.hasSelectedRecord &&
        input.folderFilterActive &&
        !input.selectedFolderPath.empty()) {
        selection.preferredFolderPath = input.selectedFolderPath;
    }
    selection.fallbackToFirstResult = input.hasSelectedRecord;
    return selection;
}

void HandleAsyncSearchBrowsePreviewRestore(
    const AsyncSearchBrowsePreviewRestoreInput& input,
    AsyncSearchBrowsePreviewEffects& effects) {
    if (!input.databaseView || !input.integratedMode || !input.hasActiveBrowseScope) return;
    const int index = aura::coverwall::ResolveBrowseSurfacePreviewIndex(
        input.records,
        input.selection,
        [](const AsyncSearchBrowsePreviewRecord& record) { return record.fileId; },
        [](const AsyncSearchBrowsePreviewRecord& record) -> const std::string& {
            return record.filePath;
        },
        [](const AsyncSearchBrowsePreviewRecord& record) -> const std::string& {
            return record.canonicalFolderPath;
        });
    if (index >= 0 && index < static_cast<int>(input.records.size())) {
        effects.selectResult(index);
    }
}

}  // namespace aura::search

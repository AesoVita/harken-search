#include "AsyncSearchBrowsePreviewWorkflow.h"

#include <cassert>

struct Effects final : aura::search::AsyncSearchBrowsePreviewEffects {
    int selectedIndex = -1;

    void selectResult(int index) override { selectedIndex = index; }
};

int main() {
    aura::search::AsyncSearchBrowsePreviewCaptureInput capture;
    capture.hasSelectedRecord = true;
    capture.selectedFileId = 11;
    capture.selectedFilePath = "old.wav";
    capture.integratedMode = true;
    capture.coverFilterActive = true;
    capture.coverFilterKey = "cover";
    capture.pinnedCoverKey = "cover";
    capture.pinnedFilePath = "preferred.wav";
    capture.folderFilterActive = true;
    capture.selectedFolderPath = "Folder";

    const auto selection = aura::search::CaptureAsyncSearchBrowsePreview(capture);
    assert(selection.preservedFileId == 11);
    assert(selection.preservedFilePath == "old.wav");
    assert(selection.preferredFilePath == "preferred.wav");
    assert(selection.preferredFolderPath == "Folder");
    assert(selection.fallbackToFirstResult);

    aura::search::AsyncSearchBrowsePreviewRestoreInput restore;
    restore.databaseView = true;
    restore.integratedMode = true;
    restore.hasActiveBrowseScope = true;
    restore.selection = selection;
    restore.records = {
        {10, "first.wav", "Other"},
        {11, "old.wav", "Folder"},
        {12, "preferred.wav", "Folder"},
    };

    Effects effects;
    aura::search::HandleAsyncSearchBrowsePreviewRestore(restore, effects);
    assert(effects.selectedIndex == 2);

    effects.selectedIndex = -1;
    restore.databaseView = false;
    aura::search::HandleAsyncSearchBrowsePreviewRestore(restore, effects);
    assert(effects.selectedIndex == -1);
    return 0;
}

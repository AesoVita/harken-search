#pragma once

#include "CoverBrowseSurfaceState.h"

#include <cstdint>
#include <string>
#include <vector>

namespace aura::search {

struct AsyncSearchBrowsePreviewRecord {
    std::int64_t fileId = -1;
    std::string filePath;
    std::string canonicalFolderPath;
};

struct AsyncSearchBrowsePreviewCaptureInput {
    bool hasSelectedRecord = false;
    std::int64_t selectedFileId = -1;
    std::string selectedFilePath;
    bool integratedMode = false;
    bool coverFilterActive = false;
    std::string coverFilterKey;
    std::string pinnedCoverKey;
    std::string pinnedFilePath;
    bool folderFilterActive = false;
    std::string selectedFolderPath;
};

struct AsyncSearchBrowsePreviewRestoreInput {
    bool databaseView = false;
    bool integratedMode = false;
    bool hasActiveBrowseScope = false;
    aura::coverwall::BrowseSurfacePreviewSelection selection;
    std::vector<AsyncSearchBrowsePreviewRecord> records;
};

class AsyncSearchBrowsePreviewEffects {
public:
    virtual ~AsyncSearchBrowsePreviewEffects() = default;
    virtual void selectResult(int index) = 0;
};

aura::coverwall::BrowseSurfacePreviewSelection CaptureAsyncSearchBrowsePreview(
    const AsyncSearchBrowsePreviewCaptureInput& input);

void HandleAsyncSearchBrowsePreviewRestore(
    const AsyncSearchBrowsePreviewRestoreInput& input,
    AsyncSearchBrowsePreviewEffects& effects);

}  // namespace aura::search

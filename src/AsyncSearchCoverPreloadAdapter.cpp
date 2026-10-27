#include "AsyncSearchCoverPreloadAdapter.h"

#include <utility>

namespace aura::search {

AsyncSearchCoverPreloadAdapter::AsyncSearchCoverPreloadAdapter(
    RecordProvider records,
    FolderPreloader preloadFolder,
    EmbeddedPreloader preloadEmbedded)
    : records_(std::move(records)),
      preloadFolder_(std::move(preloadFolder)),
      preloadEmbedded_(std::move(preloadEmbedded)) {}

void AsyncSearchCoverPreloadAdapter::preloadResult(std::size_t index) {
    AsyncSearchCoverPreloadRecord record;
    if (!records_(index, record)) return;
    if (!record.folderPath.empty() &&
        folders_.insert(record.folderPath).second) {
        preloadFolder_(record.folderPath);
    }
    if (!record.embeddedCoverKey.empty() && !record.filePath.empty()) {
        preloadEmbedded_(record.embeddedCoverKey, record.filePath);
    }
}

}  // namespace aura::search

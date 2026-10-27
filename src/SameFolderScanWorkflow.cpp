#include "SameFolderScanWorkflow.h"

#include <algorithm>
#include <utility>

namespace aura::same_folder {

SameFolderWorkflowRequest SameFolderScanWorkflow::start(
    std::string folderPathUtf8,
    std::string missingDirectoryErrorUtf8,
    SameFolderScanEffects& effects) {
    SameFolderWorkflowRequest request;
    request.generation = ++generation_;
    request.folderPathUtf8 = std::move(folderPathUtf8);
    request.missingDirectoryErrorUtf8 = std::move(missingDirectoryErrorUtf8);
    active_ = true;
    effects.started(request);
    return request;
}

void SameFolderScanWorkflow::drain(SameFolderScanChunkSource& source,
                                   SameFolderScanEffects& effects) {
    if (!active_) return;
    constexpr int kMaxChunksPerFrame = 4;
    constexpr std::size_t kMaxItemsPerFrame = 128;
    int chunkCount = 0;
    std::size_t mergedItems = 0;
    SameFolderWorkflowChunk chunk;
    while (chunkCount < kMaxChunksPerFrame &&
           mergedItems < kMaxItemsPerFrame && source.tryPopChunk(chunk)) {
        if (chunk.generation != generation_) continue;
        if (!chunk.filePaths.empty()) {
            mergedItems += chunk.filePaths.size();
            ++chunkCount;
            effects.append(chunk.filePaths);
        }
        if (chunk.done) {
            active_ = false;
            effects.finished(chunk.error);
            break;
        }
    }
}

}  // namespace aura::same_folder

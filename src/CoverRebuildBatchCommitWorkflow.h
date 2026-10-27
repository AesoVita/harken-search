#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace aura::cover_rebuild {

struct CoverRebuildBatchCacheUpdate {
    std::string filePath;
    std::string folderCoverKey;
    std::string folderCoverSrcPath;
    std::int64_t folderCoverSrcMtime = 0;
};

struct CoverRebuildBatchCommitInput {
    std::vector<std::string> setHasCoverPaths;
    std::vector<std::string> clearHasCoverPaths;
    std::vector<CoverRebuildBatchCacheUpdate> cacheUpdates;
};

class CoverRebuildBatchPersistencePort {
public:
    virtual ~CoverRebuildBatchPersistencePort() = default;
    virtual void setHasCover(const std::vector<std::string>& paths) = 0;
    virtual void clearHasCover(const std::vector<std::string>& paths) = 0;
    virtual void updateCoverKeyCache(
        const std::vector<CoverRebuildBatchCacheUpdate>& updates) = 0;
};

void CommitCoverRebuildBatch(
    CoverRebuildBatchCommitInput input,
    CoverRebuildBatchPersistencePort& persistence);

}  // namespace aura::cover_rebuild

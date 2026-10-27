#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace aura::cover_rebuild {

struct CoverRebuildRecord {
    std::string filePath;
    std::string folderPath;
    std::string embeddedCoverKey;
    bool hasCover = false;
};

struct FolderCoverResolution {
    std::string key;
    std::string sourcePath;
    std::int64_t sourceMtime = 0;
    bool budgetExhausted = false;
};

class CoverRebuildFolderLookupPort {
public:
    virtual ~CoverRebuildFolderLookupPort() = default;
    virtual FolderCoverResolution resolve(const std::string& folderPath) = 0;
};

struct CoverKeyCacheUpdate {
    std::string filePath;
    std::string folderCoverKey;
    std::string folderCoverSrcPath;
    std::int64_t folderCoverSrcMtime = 0;
};

struct CoverRebuildRecordResult {
    int foundDelta = 0;
    bool budgetExhausted = false;
    bool setHasCover = false;
    bool clearHasCover = false;
    std::optional<CoverKeyCacheUpdate> folderUpdate;
};

CoverRebuildRecordResult ProcessCoverRebuildRecord(
    const CoverRebuildRecord& record, CoverRebuildFolderLookupPort& lookup);

}  // namespace aura::cover_rebuild

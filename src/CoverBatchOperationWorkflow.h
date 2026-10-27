#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace aura::cover_batch {

struct CoverBatchOperationInput {
    bool active;
    bool paste;
    int progress;
    int total;
    const std::vector<std::string>& targets;
    const std::vector<std::uint8_t>& imageData;
    const std::string& mimeHint;
};

class CoverBatchPersistencePort {
public:
    virtual ~CoverBatchPersistencePort() = default;
    virtual bool hasCoverThumb(const std::string& key) = 0;
    virtual void saveCoverThumb(const std::string& key,
                                const std::vector<std::uint8_t>& data,
                                const std::string& mime) = 0;
    virtual void assignEmbeddedCoverByPaths(
        const std::vector<std::string>& paths,
        const std::string& key,
        std::int64_t timestamp) = 0;
    virtual void clearCoverByPaths(const std::vector<std::string>& paths) = 0;
    virtual std::int64_t currentUnixTime() = 0;
};

class CoverBatchViewPort {
public:
    virtual ~CoverBatchViewPort() = default;
    virtual void applyVisibleRows(const std::vector<std::string>& paths,
                                  bool paste,
                                  const std::string& key) = 0;
    virtual void invalidateFileCoverCaches(
        const std::vector<std::string>& paths) = 0;
    virtual void clearFolderCoverNegativeCache() = 0;
    virtual void requestCoverWallRefresh() = 0;
    virtual void syncToTab() = 0;
};

class CoverBatchStatePort {
public:
    virtual ~CoverBatchStatePort() = default;
    virtual void finishBatch(int total) = 0;
};

std::string ResolveCoverImageMime(const std::vector<std::uint8_t>& data,
                                  const std::string& hintMime);

bool PumpCoverBatchOperation(const CoverBatchOperationInput& input,
                             CoverBatchPersistencePort& persistence,
                             CoverBatchViewPort& view,
                             CoverBatchStatePort& state);

}  // namespace aura::cover_batch

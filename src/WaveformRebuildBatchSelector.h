#pragma once

#include "Database.h"

#include <cstdint>
#include <vector>

namespace aura::waveform_rebuild {

struct WaveformRebuildBatchRequest {
    bool fullMode = false;
    bool incrementalMiniOnly = false;
    int limit = 0;
    int offset = 0;
};

struct WaveformRebuildBatchSelection {
    std::vector<AudioRecord> records;
    bool miniOnly = false;
};

class WaveformRebuildBatchSource {
public:
    virtual ~WaveformRebuildBatchSource() = default;

    virtual std::vector<AudioRecord> getAllFiles(int limit, int offset) = 0;
    virtual bool usesMiniOnlyCacheMode() const = 0;
    virtual std::vector<int64_t> getFilesNeedingWaveformCache(int limit) = 0;
    virtual std::vector<int64_t> getFilesNeedingDetailCache(int limit) = 0;
    virtual std::vector<AudioRecord> getFilesByIds(
        const std::vector<int64_t>& ids) = 0;
};

class DatabaseWaveformRebuildBatchSource final
    : public WaveformRebuildBatchSource {
public:
    explicit DatabaseWaveformRebuildBatchSource(Database& database) noexcept;

    std::vector<AudioRecord> getAllFiles(int limit, int offset) override;
    bool usesMiniOnlyCacheMode() const override;
    std::vector<int64_t> getFilesNeedingWaveformCache(int limit) override;
    std::vector<int64_t> getFilesNeedingDetailCache(int limit) override;
    std::vector<AudioRecord> getFilesByIds(
        const std::vector<int64_t>& ids) override;

private:
    Database& database_;
};

WaveformRebuildBatchSelection SelectWaveformRebuildBatch(
    WaveformRebuildBatchSource& source,
    const WaveformRebuildBatchRequest& request);

}  // namespace aura::waveform_rebuild

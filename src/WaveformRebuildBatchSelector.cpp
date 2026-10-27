#include "WaveformRebuildBatchSelector.h"

namespace aura::waveform_rebuild {

DatabaseWaveformRebuildBatchSource::DatabaseWaveformRebuildBatchSource(
    Database& database) noexcept
    : database_(database) {}

std::vector<AudioRecord> DatabaseWaveformRebuildBatchSource::getAllFiles(
    int limit,
    int offset) {
    return database_.getAllFiles(limit, offset);
}

bool DatabaseWaveformRebuildBatchSource::usesMiniOnlyCacheMode() const {
    return database_.getWaveformCacheMode() == WaveformCachePolicy::Mode::MiniOnly;
}

std::vector<int64_t>
DatabaseWaveformRebuildBatchSource::getFilesNeedingWaveformCache(int limit) {
    return database_.getFilesNeedingWaveformCache(limit);
}

std::vector<int64_t>
DatabaseWaveformRebuildBatchSource::getFilesNeedingDetailCache(int limit) {
    return database_.getFilesNeedingDetailCache(limit);
}

std::vector<AudioRecord> DatabaseWaveformRebuildBatchSource::getFilesByIds(
    const std::vector<int64_t>& ids) {
    return database_.getFilesByIds(ids);
}

WaveformRebuildBatchSelection SelectWaveformRebuildBatch(
    WaveformRebuildBatchSource& source,
    const WaveformRebuildBatchRequest& request) {
    if (request.fullMode) {
        return {
            source.getAllFiles(request.limit, request.offset),
            source.usesMiniOnlyCacheMode(),
        };
    }

    if (request.incrementalMiniOnly) {
        auto ids = source.getFilesNeedingWaveformCache(request.limit);
        const bool miniOnly = !ids.empty();
        return {source.getFilesByIds(ids), miniOnly};
    }

    auto ids = source.getFilesNeedingDetailCache(request.limit);
    bool miniOnly = false;
    if (ids.empty()) {
        ids = source.getFilesNeedingWaveformCache(request.limit);
        miniOnly = !ids.empty();
    }
    return {source.getFilesByIds(ids), miniOnly};
}

}  // namespace aura::waveform_rebuild

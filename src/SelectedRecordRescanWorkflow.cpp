#include "SelectedRecordRescanWorkflow.h"

namespace aura::record_rescan {
namespace {

void ApplyMetadata(
    SelectedRecordRescanUpdate& update,
    const SelectedRecordRescanMetadata& metadata) {
    if (!metadata.title.empty()) update.title = metadata.title;
    if (!metadata.artist.empty()) update.artist = metadata.artist;
    if (!metadata.album.empty()) update.album = metadata.album;
    if (!metadata.description.empty()) update.description = metadata.description;
    if (!metadata.category.empty()) {
        update.category = metadata.category;
    } else if (!metadata.genre.empty()) {
        update.category = metadata.genre;
    }
    if (!metadata.keywords.empty()) {
        update.keywords = metadata.keywords;
    } else if (!metadata.comment.empty()) {
        update.keywords = metadata.comment;
    }
    for (const auto& [key, value] : metadata.extendedMetadata) {
        if (!value.empty()) update.extendedMetadata[key] = value;
    }
}

}  // namespace

SelectedRecordRescanItemResult RescanSelectedRecord(
    const SelectedRecordRescanItem& item,
    SelectedRecordRescanOperationPort& port) {
    const auto file = port.observeFile(item.filePath);
    if (!file.regular || !file.sizeAvailable) {
        return SelectedRecordRescanItemResult::NotUpdated;
    }

    SelectedRecordRescanUpdate update;
    update.title = item.title;
    update.artist = item.artist;
    update.album = item.album;
    update.description = item.description;
    update.category = item.category;
    update.keywords = item.keywords;
    update.extendedMetadata = item.extendedMetadata;
    update.sampleRate = item.sampleRate;
    update.channels = item.channels;
    update.totalFrames = item.totalFrames;
    update.durationSeconds = item.durationSeconds;

    SelectedRecordRescanMetadata metadata;
    if (port.readMetadata(item.filePath, metadata)) {
        ApplyMetadata(update, metadata);
    }

    const bool oldAudioInfoMissing =
        item.sampleRate == 0 || item.channels == 0 || item.totalFrames == 0 ||
        item.durationSeconds <= 0.0f;
    const bool oldWaveformWasDecoderFailure =
        port.wasDecoderInitFailure(item.filePath);

    SelectedRecordRescanAudioFacts audio{
        item.sampleRate,
        item.channels,
        item.totalFrames,
        item.durationSeconds,
    };
    const bool audioInfoOk = port.probeAudio(item.filePath, audio);
    update.sampleRate = audio.sampleRate;
    update.channels = audio.channels;
    update.totalFrames = audio.totalFrames;
    update.durationSeconds = audio.durationSeconds;
    update.fileSize = file.size;
    update.fileMtime = port.lastWriteTimeUnixSeconds(item.filePath);
    update.lastIndexed = port.nowUnixSeconds();

    const bool updated = port.updateRecord(item, update);
    if (updated && audioInfoOk &&
        (oldAudioInfoMissing || oldWaveformWasDecoderFailure)) {
        port.clearWaveformCache(item.filePath);
    }
    return updated ? SelectedRecordRescanItemResult::Updated
                   : SelectedRecordRescanItemResult::NotUpdated;
}

SelectedRecordRescanBatchResult RescanSelectedRecords(
    const std::vector<SelectedRecordRescanItem>& items,
    SelectedRecordRescanBatchPort& port) {
    SelectedRecordRescanBatchResult result;
    if (items.empty()) return result;

    if (!port.beginTransaction()) {
        result.failed = static_cast<int>(items.size());
        return result;
    }
    for (const auto& item : items) {
        switch (port.processProtected(item)) {
            case SelectedRecordRescanItemResult::Updated:
                ++result.updated;
                break;
            case SelectedRecordRescanItemResult::Failed:
                ++result.failed;
                break;
            case SelectedRecordRescanItemResult::NotUpdated:
                ++result.failed;
                break;
        }
    }
    if (!port.commitTransaction()) {
        result.updated = 0;
        result.failed = static_cast<int>(items.size());
    }
    return result;
}

}  // namespace aura::record_rescan

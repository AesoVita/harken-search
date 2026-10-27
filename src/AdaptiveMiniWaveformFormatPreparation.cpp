#include "AdaptiveMiniWaveformFormatPreparation.h"

namespace aura::waveform_rebuild {

DatabaseAdaptiveMiniFormatPreparationSource::
DatabaseAdaptiveMiniFormatPreparationSource(Database& database) noexcept
    : database_(database) {}

bool DatabaseAdaptiveMiniFormatPreparationSource::
usesAdaptiveMiniWaveform() const {
    return database_.usesAdaptiveMiniWaveform();
}

int DatabaseAdaptiveMiniFormatPreparationSource::purgeAllMiniWaveform() {
    return database_.purgeAllMiniWaveform();
}

int DatabaseAdaptiveMiniFormatPreparationSource::
countFilesNeedingWaveformCache() {
    return database_.countFilesNeedingWaveformCache();
}

AdaptiveMiniFormatPreparationResult PrepareAdaptiveMiniWaveformFormat(
    AdaptiveMiniFormatPreparationSource& source,
    AdaptiveMiniFormatFinalizationState state) {
    if (!source.usesAdaptiveMiniWaveform()) {
        state.rebuildPending = false;
        return {
            AdaptiveMiniFormatPreparationDecision::Disabled,
            state,
            0,
            0,
        };
    }

    const int purged = source.purgeAllMiniWaveform();
    const int64_t rebuildTotal = source.countFilesNeedingWaveformCache();
    return {
        rebuildTotal <= 0
            ? AdaptiveMiniFormatPreparationDecision::NoWork
            : AdaptiveMiniFormatPreparationDecision::RebuildRequired,
        state,
        purged,
        rebuildTotal,
    };
}

}  // namespace aura::waveform_rebuild

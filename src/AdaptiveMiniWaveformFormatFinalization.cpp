#include "AdaptiveMiniWaveformFormatFinalization.h"

namespace aura::waveform_rebuild {

DatabaseAdaptiveMiniFormatStampPort::DatabaseAdaptiveMiniFormatStampPort(
    Database& database) noexcept
    : database_(database) {}

bool DatabaseAdaptiveMiniFormatStampPort::markUpgradeComplete() {
    return database_.markAdaptiveMiniWaveformFormatUpgradeComplete();
}

AdaptiveMiniFormatFinalizationResult FinalizeAdaptiveMiniWaveformFormat(
    AdaptiveMiniFormatStampPort& port,
    AdaptiveMiniFormatFinalizationReason reason,
    AdaptiveMiniFormatFinalizationState state) {
    AdaptiveMiniFormatFinalizationResult result{state, false, false};

    if (reason == AdaptiveMiniFormatFinalizationReason::Cancelled) {
        if (result.state.stampPending) {
            result.state.rebuildPending = true;
            result.state.stampPending = false;
        }
        return result;
    }

    if (reason == AdaptiveMiniFormatFinalizationReason::RebuildCompleted &&
        !result.state.stampPending) {
        return result;
    }

    result.stampAttempted = true;
    result.stampSucceeded = port.markUpgradeComplete();

    if (reason == AdaptiveMiniFormatFinalizationReason::NoWork) {
        if (result.stampSucceeded) {
            result.state.rebuildPending = false;
            result.state.stampPending = false;
        }
        return result;
    }

    result.state.stampPending = false;
    if (!result.stampSucceeded) {
        result.state.rebuildPending = true;
    }
    return result;
}

}  // namespace aura::waveform_rebuild

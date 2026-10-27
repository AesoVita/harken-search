#include "WaveformRebuildCancellationRecovery.h"

namespace aura::waveform_rebuild {

AdaptiveMiniFormatFinalizationResult RecoverCancelledWaveformRebuild(
    AdaptiveMiniFormatStampPort& stampPort,
    WaveformRebuildCancellationRecoveryEffects& effects,
    AdaptiveMiniFormatFinalizationState adaptiveState) {
    auto result = FinalizeAdaptiveMiniWaveformFormat(
        stampPort,
        AdaptiveMiniFormatFinalizationReason::Cancelled,
        adaptiveState);
    effects.applyAdaptiveState(result.state);
    effects.restartForegroundWorker();
    effects.resetRuntimeState();
    return result;
}

}  // namespace aura::waveform_rebuild

#pragma once

#include "AdaptiveMiniWaveformFormatFinalization.h"

namespace aura::waveform_rebuild {

class WaveformRebuildCancellationRecoveryEffects {
public:
    virtual ~WaveformRebuildCancellationRecoveryEffects() = default;

    virtual void applyAdaptiveState(
        const AdaptiveMiniFormatFinalizationState& state) = 0;
    virtual void restartForegroundWorker() = 0;
    virtual void resetRuntimeState() = 0;
};

AdaptiveMiniFormatFinalizationResult RecoverCancelledWaveformRebuild(
    AdaptiveMiniFormatStampPort& stampPort,
    WaveformRebuildCancellationRecoveryEffects& effects,
    AdaptiveMiniFormatFinalizationState adaptiveState);

}  // namespace aura::waveform_rebuild

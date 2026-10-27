#pragma once

#include "WaveformRebuildCancellationRecovery.h"

struct AppState;

namespace aura::waveform_rebuild {

class WaveformRebuildRuntimeState;

class WaveformRebuildCancellationRecoveryStateAdapter final
    : private WaveformRebuildCancellationRecoveryEffects {
public:
    WaveformRebuildCancellationRecoveryStateAdapter(
        AppState& state,
        WaveformRebuildRuntimeState& runtimeState);

    AdaptiveMiniFormatFinalizationResult recover(
        AdaptiveMiniFormatStampPort& stampPort,
        AdaptiveMiniFormatFinalizationState adaptiveState);

private:
    void applyAdaptiveState(
        const AdaptiveMiniFormatFinalizationState& adaptiveState) override;
    void restartForegroundWorker() override;
    void resetRuntimeState() override;

    AppState& state_;
    WaveformRebuildRuntimeState& runtimeState_;
};

}  // namespace aura::waveform_rebuild

#include "src/AdaptiveMiniWaveformRebuildActivation.h"

#include <cstdio>

namespace {

int g_failures = 0;

void Check(bool condition, const char* message) {
    if (condition) {
        std::printf("PASS: %s\n", message);
    } else {
        std::printf("FAIL: %s\n", message);
        ++g_failures;
    }
}

bool SameState(
    const aura::waveform_rebuild::AdaptiveMiniWaveformRebuildActivationState& left,
    const aura::waveform_rebuild::AdaptiveMiniWaveformRebuildActivationState& right) {
    return left.waveformRebuildMiniOnlyMode == right.waveformRebuildMiniOnlyMode &&
           left.incrementalMiniOnly == right.incrementalMiniOnly &&
           left.rebuildPending == right.rebuildPending &&
           left.stampPending == right.stampPending;
}

}  // namespace

int main() {
    using aura::waveform_rebuild::ActivateAdaptiveMiniWaveformRebuild;
    using aura::waveform_rebuild::AdaptiveMiniWaveformRebuildActivationState;

    const AdaptiveMiniWaveformRebuildActivationState failedInput{
        true, false, true, false};
    auto result = ActivateAdaptiveMiniWaveformRebuild(false, failedInput);
    Check(!result.activated, "failed start reports no activation");
    Check(SameState(result.state, failedInput),
          "failed start preserves every input state value");

    result = ActivateAdaptiveMiniWaveformRebuild(
        true, AdaptiveMiniWaveformRebuildActivationState{});
    Check(result.activated, "successful start reports activation");
    Check(result.state.waveformRebuildMiniOnlyMode &&
              result.state.incrementalMiniOnly &&
              !result.state.rebuildPending && result.state.stampPending,
          "successful start applies the exact activation transition");

    result = ActivateAdaptiveMiniWaveformRebuild(
        true, AdaptiveMiniWaveformRebuildActivationState{false, true, true, false});
    Check(result.state.waveformRebuildMiniOnlyMode &&
              result.state.incrementalMiniOnly &&
              !result.state.rebuildPending && result.state.stampPending,
          "successful start overwrites mixed state deterministically");

    std::printf("\n=== Adaptive Mini Waveform Rebuild Activation: %d failure(s) ===\n",
                g_failures);
    return g_failures == 0 ? 0 : 1;
}

#include "src/AdaptiveMiniWaveformFormatFinalization.h"

#include <cstdio>

bool Database::markAdaptiveMiniWaveformFormatUpgradeComplete() {
    return true;
}

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

class FakeStampPort final
    : public aura::waveform_rebuild::AdaptiveMiniFormatStampPort {
public:
    bool returnValue = false;
    int calls = 0;

    bool markUpgradeComplete() override {
        ++calls;
        return returnValue;
    }
};

}  // namespace

int main() {
    using aura::waveform_rebuild::AdaptiveMiniFormatFinalizationReason;
    using aura::waveform_rebuild::AdaptiveMiniFormatFinalizationState;
    using aura::waveform_rebuild::FinalizeAdaptiveMiniWaveformFormat;

    FakeStampPort noWorkSuccess;
    noWorkSuccess.returnValue = true;
    auto result = FinalizeAdaptiveMiniWaveformFormat(
        noWorkSuccess,
        AdaptiveMiniFormatFinalizationReason::NoWork,
        AdaptiveMiniFormatFinalizationState{true, true});
    Check(noWorkSuccess.calls == 1 && result.stampAttempted && result.stampSucceeded,
          "no-work success stamps exactly once");
    Check(!result.state.rebuildPending && !result.state.stampPending,
          "no-work success clears both pending flags");

    FakeStampPort noWorkFailure;
    result = FinalizeAdaptiveMiniWaveformFormat(
        noWorkFailure,
        AdaptiveMiniFormatFinalizationReason::NoWork,
        AdaptiveMiniFormatFinalizationState{true, false});
    Check(noWorkFailure.calls == 1 && result.stampAttempted && !result.stampSucceeded,
          "no-work failure reports one failed stamp");
    Check(result.state.rebuildPending && !result.state.stampPending,
          "no-work failure preserves both input flags");

    FakeStampPort completedNoop;
    result = FinalizeAdaptiveMiniWaveformFormat(
        completedNoop,
        AdaptiveMiniFormatFinalizationReason::RebuildCompleted,
        AdaptiveMiniFormatFinalizationState{false, false});
    Check(completedNoop.calls == 0 && !result.stampAttempted,
          "completed rebuild without pending stamp is a no-op");
    Check(!result.state.rebuildPending && !result.state.stampPending,
          "completed no-op preserves state");

    FakeStampPort completedSuccess;
    completedSuccess.returnValue = true;
    result = FinalizeAdaptiveMiniWaveformFormat(
        completedSuccess,
        AdaptiveMiniFormatFinalizationReason::RebuildCompleted,
        AdaptiveMiniFormatFinalizationState{true, true});
    Check(completedSuccess.calls == 1 && result.stampSucceeded,
          "completed rebuild stamps exactly once");
    Check(result.state.rebuildPending && !result.state.stampPending,
          "completed success preserves rebuild pending and consumes stamp pending");

    FakeStampPort completedFailure;
    result = FinalizeAdaptiveMiniWaveformFormat(
        completedFailure,
        AdaptiveMiniFormatFinalizationReason::RebuildCompleted,
        AdaptiveMiniFormatFinalizationState{false, true});
    Check(completedFailure.calls == 1 && result.stampAttempted && !result.stampSucceeded,
          "completed failure reports one failed stamp");
    Check(result.state.rebuildPending && !result.state.stampPending,
          "completed failure requests retry and consumes stamp pending");

    FakeStampPort cancelledNoop;
    result = FinalizeAdaptiveMiniWaveformFormat(
        cancelledNoop,
        AdaptiveMiniFormatFinalizationReason::Cancelled,
        AdaptiveMiniFormatFinalizationState{false, false});
    Check(cancelledNoop.calls == 0 && !result.stampAttempted,
          "cancel without pending stamp is a no-op");

    FakeStampPort cancelled;
    cancelled.returnValue = true;
    result = FinalizeAdaptiveMiniWaveformFormat(
        cancelled,
        AdaptiveMiniFormatFinalizationReason::Cancelled,
        AdaptiveMiniFormatFinalizationState{false, true});
    Check(cancelled.calls == 0 && !result.stampAttempted,
          "cancel never writes the format stamp");
    Check(result.state.rebuildPending && !result.state.stampPending,
          "cancel requests retry and consumes stamp pending");

    std::printf("\n=== Adaptive Mini Waveform Format Finalization: %d failure(s) ===\n",
                g_failures);
    return g_failures == 0 ? 0 : 1;
}

#include "src/WaveformRebuildCancellationRecovery.h"

#include <cstdio>
#include <string>
#include <vector>

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
    int calls = 0;

    bool markUpgradeComplete() override {
        ++calls;
        return true;
    }
};

class FakeRecoveryEffects final
    : public aura::waveform_rebuild::
          WaveformRebuildCancellationRecoveryEffects {
public:
    std::vector<std::string> trace;
    aura::waveform_rebuild::AdaptiveMiniFormatFinalizationState applied;
    int applyCalls = 0;
    int restartCalls = 0;
    int resetCalls = 0;

    void applyAdaptiveState(
        const aura::waveform_rebuild::
            AdaptiveMiniFormatFinalizationState& state) override {
        applied = state;
        ++applyCalls;
        trace.push_back("applyAdaptiveState");
    }

    void restartForegroundWorker() override {
        ++restartCalls;
        trace.push_back("restartForegroundWorker");
    }

    void resetRuntimeState() override {
        ++resetCalls;
        trace.push_back("resetRuntimeState");
    }
};

bool SameState(
    const aura::waveform_rebuild::AdaptiveMiniFormatFinalizationState& lhs,
    const aura::waveform_rebuild::AdaptiveMiniFormatFinalizationState& rhs) {
    return lhs.rebuildPending == rhs.rebuildPending &&
           lhs.stampPending == rhs.stampPending;
}

}  // namespace

int main() {
    using aura::waveform_rebuild::
        AdaptiveMiniFormatFinalizationState;
    using aura::waveform_rebuild::RecoverCancelledWaveformRebuild;

    const std::vector<std::string> expectedTrace{
        "applyAdaptiveState",
        "restartForegroundWorker",
        "resetRuntimeState",
    };

    FakeStampPort noPendingStamp;
    FakeRecoveryEffects noPendingEffects;
    const auto noPending = RecoverCancelledWaveformRebuild(
        noPendingStamp,
        noPendingEffects,
        AdaptiveMiniFormatFinalizationState{true, false});
    Check(noPendingStamp.calls == 0 && !noPending.stampAttempted &&
              !noPending.stampSucceeded && noPending.state.rebuildPending &&
              !noPending.state.stampPending,
          "cancel without pending stamp preserves state and suppresses stamp");
    Check(SameState(noPendingEffects.applied, noPending.state),
          "cancel applies the complete no-pending state");
    Check(noPendingEffects.trace == expectedTrace &&
              noPendingEffects.applyCalls == 1 &&
              noPendingEffects.restartCalls == 1 &&
              noPendingEffects.resetCalls == 1,
          "no-pending cancel performs each recovery effect once in order");

    FakeStampPort pendingStamp;
    FakeRecoveryEffects pendingEffects;
    const auto pending = RecoverCancelledWaveformRebuild(
        pendingStamp,
        pendingEffects,
        AdaptiveMiniFormatFinalizationState{false, true});
    Check(pendingStamp.calls == 0 && !pending.stampAttempted &&
              !pending.stampSucceeded && pending.state.rebuildPending &&
              !pending.state.stampPending,
          "cancel with pending stamp requests retry without writing stamp");
    Check(SameState(pendingEffects.applied, pending.state),
          "cancel applies the complete pending-stamp state");
    Check(pendingEffects.trace == expectedTrace &&
              pendingEffects.applyCalls == 1 &&
              pendingEffects.restartCalls == 1 &&
              pendingEffects.resetCalls == 1,
          "pending-stamp cancel preserves finalization restart reset order");

    std::printf("\n=== Waveform Rebuild Cancellation Recovery: %d failure(s) ===\n",
                g_failures);
    return g_failures == 0 ? 0 : 1;
}

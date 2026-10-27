#include "src/AdaptiveMiniWaveformFormatPreparation.h"

#include <cstdio>
#include <string>
#include <vector>

int Database::purgeAllMiniWaveform() {
    return 0;
}

int Database::countFilesNeedingWaveformCache() {
    return 0;
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

class FakeSource final
    : public aura::waveform_rebuild::AdaptiveMiniFormatPreparationSource {
public:
    bool adaptive = false;
    int purged = 0;
    int rebuildTotal = 0;
    mutable std::vector<std::string> calls;

    bool usesAdaptiveMiniWaveform() const override {
        calls.push_back("uses");
        return adaptive;
    }

    int purgeAllMiniWaveform() override {
        calls.push_back("purge");
        return purged;
    }

    int countFilesNeedingWaveformCache() override {
        calls.push_back("count");
        return rebuildTotal;
    }
};

}  // namespace

int main() {
    using aura::waveform_rebuild::AdaptiveMiniFormatFinalizationState;
    using aura::waveform_rebuild::AdaptiveMiniFormatPreparationDecision;
    using aura::waveform_rebuild::PrepareAdaptiveMiniWaveformFormat;

    FakeSource disabled;
    auto result = PrepareAdaptiveMiniWaveformFormat(
        disabled, AdaptiveMiniFormatFinalizationState{true, true});
    Check(result.decision == AdaptiveMiniFormatPreparationDecision::Disabled,
          "unsupported format returns disabled");
    Check(disabled.calls == std::vector<std::string>{"uses"},
          "unsupported format skips purge and count");
    Check(!result.state.rebuildPending && result.state.stampPending,
          "unsupported format clears only rebuild pending");

    FakeSource zero;
    zero.adaptive = true;
    zero.purged = 7;
    result = PrepareAdaptiveMiniWaveformFormat(
        zero, AdaptiveMiniFormatFinalizationState{true, false});
    Check(result.decision == AdaptiveMiniFormatPreparationDecision::NoWork,
          "zero debt returns no-work");
    Check(zero.calls == std::vector<std::string>{"uses", "purge", "count"},
          "supported format preserves uses-purge-count order");
    Check(result.purged == 7 && result.rebuildTotal == 0 &&
              result.state.rebuildPending && !result.state.stampPending,
          "zero debt preserves telemetry and pending state");

    FakeSource negative;
    negative.adaptive = true;
    negative.purged = 3;
    negative.rebuildTotal = -1;
    result = PrepareAdaptiveMiniWaveformFormat(
        negative, AdaptiveMiniFormatFinalizationState{false, true});
    Check(result.decision == AdaptiveMiniFormatPreparationDecision::NoWork,
          "negative debt preserves legacy no-work classification");
    Check(result.purged == 3 && result.rebuildTotal == -1 &&
              !result.state.rebuildPending && result.state.stampPending,
          "negative debt preserves input state and exact telemetry");

    FakeSource rebuild;
    rebuild.adaptive = true;
    rebuild.purged = 11;
    rebuild.rebuildTotal = 42;
    result = PrepareAdaptiveMiniWaveformFormat(
        rebuild, AdaptiveMiniFormatFinalizationState{true, false});
    Check(result.decision ==
              AdaptiveMiniFormatPreparationDecision::RebuildRequired,
          "positive debt requests rebuild");
    Check(rebuild.calls ==
              std::vector<std::string>{"uses", "purge", "count"},
          "positive debt performs each preparation call once");
    Check(result.purged == 11 && result.rebuildTotal == 42 &&
              result.state.rebuildPending && !result.state.stampPending,
          "positive debt preserves state and exact telemetry");

    std::printf("\n=== Adaptive Mini Waveform Format Preparation: %d failure(s) ===\n",
                g_failures);
    return g_failures == 0 ? 0 : 1;
}

#include "src/WaveformRebuildPumpGate.h"

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

}  // namespace

int main() {
    using aura::waveform_rebuild::EvaluateWaveformRebuildBatchGate;
    using aura::waveform_rebuild::EvaluateWaveformRebuildCapacityGate;
    using aura::waveform_rebuild::ShouldSampleWaveformRebuildDrain;
    using aura::waveform_rebuild::WaveformRebuildBatchGateDecision;
    using aura::waveform_rebuild::WaveformRebuildBatchGateInput;
    using aura::waveform_rebuild::WaveformRebuildCapacityGateInput;

    auto capacity = EvaluateWaveformRebuildCapacityGate(
        WaveformRebuildCapacityGateInput{3, 4, 5, 12});
    Check(capacity.wait && capacity.pending == 12,
          "exact threshold waits with exact pending sum");

    capacity = EvaluateWaveformRebuildCapacityGate(
        WaveformRebuildCapacityGateInput{2, 3, 4, 10});
    Check(!capacity.wait && capacity.pending == 9,
          "below threshold continues with exact pending sum");

    auto decision = EvaluateWaveformRebuildBatchGate(
        WaveformRebuildBatchGateInput{1, 2, 8, 9, 7});
    Check(!ShouldSampleWaveformRebuildDrain(
              WaveformRebuildBatchGateInput{1, 2, 8, 9, 0}),
          "current batch skips the drain sample");
    Check(decision == WaveformRebuildBatchGateDecision::ContinueBatch,
          "non-exhausted batch continues despite worker pending");

    decision = EvaluateWaveformRebuildBatchGate(
        WaveformRebuildBatchGateInput{0, 0, 0, 0, 7});
    Check(!ShouldSampleWaveformRebuildDrain(
              WaveformRebuildBatchGateInput{0, 0, 0, 0, 0}),
          "first empty batch skips the drain sample");
    Check(decision == WaveformRebuildBatchGateDecision::LoadBatch,
          "first empty batch loads despite worker pending");

    decision = EvaluateWaveformRebuildBatchGate(
        WaveformRebuildBatchGateInput{2, 2, 1, 0, 3});
    Check(ShouldSampleWaveformRebuildDrain(
              WaveformRebuildBatchGateInput{2, 2, 1, 0, 0}),
          "submitted exhausted batch requests a drain sample");
    Check(decision == WaveformRebuildBatchGateDecision::WaitForDrain,
          "submitted exhausted batch waits for worker drain");

    decision = EvaluateWaveformRebuildBatchGate(
        WaveformRebuildBatchGateInput{2, 2, 0, 4, 3});
    Check(decision == WaveformRebuildBatchGateDecision::WaitForDrain,
          "offset also marks previously submitted work");

    decision = EvaluateWaveformRebuildBatchGate(
        WaveformRebuildBatchGateInput{2, 2, 4, 5, 0});
    Check(decision == WaveformRebuildBatchGateDecision::LoadBatch,
          "drained exhausted batch loads next batch");

    std::printf("\n=== Waveform Rebuild Pump Gate: %d failure(s) ===\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}

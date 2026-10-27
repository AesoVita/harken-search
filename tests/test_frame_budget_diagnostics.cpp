#include "FrameBudgetDiagnostics.h"

#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <string>
using namespace aura::diagnostics;
namespace {
int failures = 0;
void Check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}
FrameTiming ValidTiming() { return {1.0, 5.0, 5.25, 6.25, 16.0, true, true, true, true}; }
FrameBudgetEnvironment ValidEnvironment() {
    return {true, true, false, 1280, 720, 1.0, true, true, true};
}
void TestSummaryStatistics() {
    const auto empty = FrameBudgetDiagnostics::summarize(nullptr, 0);
    Check(!empty.valid, "empty summary is invalid");
    Check(std::isnan(FrameBudgetDiagnostics::nearestRank(nullptr, 0, 0.95)),
          "empty nearestRank is NaN");
    const std::array<double, 4> even{4.0, 1.0, 3.0, 2.0};
    const auto evenStats = FrameBudgetDiagnostics::summarize(even.data(), even.size());
    Check(evenStats.valid && evenStats.median_ms == 2.5, "even median is deterministic");
    std::array<double, 20> ordered{};
    for (std::size_t i = 0; i < ordered.size(); ++i) ordered[i] = double(i + 1);
    const auto stats = FrameBudgetDiagnostics::summarize(ordered.data(), ordered.size());
    Check(stats.p95_ms == 19.0, "p95 uses nearest rank");
    Check(stats.max_ms == 20.0, "max preserves the largest sample");
}
void TestExactScenarioCapacitiesAndTransitions() {
    using Clock = FrameBudgetDiagnostics::Clock;
    const auto start = Clock::time_point{};
    FrameBudgetDiagnostics capture;
    Check(FrameBudgetDiagnostics::warmupFrameCount() == 60, "warm-up capacity is 60");
    Check(FrameBudgetDiagnostics::sampleFrameCount() == 240, "sample capacity is 240");
    Check(capture.startCapture(start, ValidEnvironment()), "capture starts once");
    Check(capture.beginScenario(FrameScenario::Idle), "idle starts first");
    for (std::size_t i = 0; i < 60; ++i) capture.recordFrame(ValidTiming(), start);
    Check(capture.result().idleSamples.count == 0, "warm-up frames are not sampled");
    for (std::size_t i = 0; i < 240; ++i) capture.recordFrame(ValidTiming(), start);
    Check(capture.state() == FrameCaptureState::AwaitingBusy, "idle completion awaits busy");
    Check(capture.beginScenario(FrameScenario::BusyScrollMiniPlayback), "busy follows idle");
    for (std::size_t i = 0; i < 300; ++i) {
        if (i >= 60) {
            capture.observeBusyFrame(int((i - 60) % 120), true, false, 20);
            capture.observeMiniActivity(1, 1);
        }
        capture.recordFrame(ValidTiming(), start);
    }
    const auto result = capture.result();
    Check(capture.state() == FrameCaptureState::Complete, "busy completion ends capture");
    Check(result.idleSamples.count == 240 && result.busySamples.count == 240,
          "idle and busy retain fixed arrays");
    Check(result.busySamples.renderUiFrames == 240 && result.busySamples.glRenderFrames == 240 &&
          result.busySamples.presentFrames == 240, "busy phase marks are counted from accepted frames");
    Check(result.busySamples.cpuWorkSamplesMs[0] == 4.0 &&
          result.busySamples.presentBlockSamplesMs[0] == 1.0 &&
          result.busySamples.frameIntervalSamplesMs[0] == 16.0,
          "CPU, Present, and interval samples stay separate");
    Check(!capture.recordFrame(ValidTiming(), start), "overflow frame is rejected");
    Check(capture.result().busySamples.count == 240, "overflow cannot change samples");
    const std::string json = capture.resultJson();
    Check(json.find("\"status\":\"measured\"") != std::string::npos,
          "complete authentic workload emits measured JSON");
    Check(json.find("\"render_ui_frames\":240") != std::string::npos &&
          json.find("\"gl_render_frames\":240") != std::string::npos &&
          json.find("\"present_frames\":240") != std::string::npos, "measured JSON emits actual phase counts");
    Check(json.find("\"value_ms\":4") != std::string::npos,
          "value is busy CPU p95");
}
void TestEnvironmentGate() {
    FrameBudgetDiagnostics capture;
    auto environment = ValidEnvironment();
    environment.client_width = 1279;
    Check(!capture.startCapture(FrameBudgetDiagnostics::Clock::time_point{}, environment),
          "invalid preflight is rejected");
    const std::string json = capture.resultJson();
    Check(json.find("environment_gate") != std::string::npos &&
          json.find("\"reason\":\"environment_invariant\"") != std::string::npos &&
          json.find("\"stage\":\"preflight\"") != std::string::npos,
          "invalid preflight reports environment gate");
    Check(json.find("\"scenarios\"") == std::string::npos &&
          json.find("\"value_ms\"") == std::string::npos &&
          json.find("\"budget_ms\"") == std::string::npos,
          "environment gate omits zero-looking measurements");
}
void TestEnvironmentLossDiagnostics() {
    using Clock = FrameBudgetDiagnostics::Clock;
    struct Case {
        const char* name;
        void (*mutate)(FrameBudgetEnvironment&);
        const char* failedField;
        const char* observedValue;
    };
    const std::array<Case, 9> cases{{
        {"visible", [](auto& e) { e.window_visible = false; }, "window_visible", "\"window_visible\":false"},
        {"focused", [](auto& e) { e.window_focused = false; }, "window_focused", "\"window_focused\":false"},
        {"iconic", [](auto& e) { e.window_iconic = true; }, "window_iconic", "\"window_iconic\":true"},
        {"width", [](auto& e) { e.client_width = 1279; }, "client_width_changed", "\"client_width\":1279"},
        {"height", [](auto& e) { e.client_height = 719; }, "client_height_changed", "\"client_height\":719"},
        {"dpi", [](auto& e) { e.dpi_scale = 1.25; }, "dpi_scale_changed", "\"dpi_scale\":1.25"},
        {"opengl", [](auto& e) { e.opengl = false; }, "opengl", "\"opengl\":false"},
        {"vsync", [](auto& e) { e.vsync = false; }, "vsync", "\"vsync\":false"},
        {"audio", [](auto& e) { e.audio_device = false; }, "audio_device", "\"audio_device\":false"},
    }};
    for (const auto& item : cases) {
        FrameBudgetDiagnostics capture;
        Check(capture.startCapture(Clock::time_point{}, ValidEnvironment()) &&
              capture.beginScenario(FrameScenario::Idle), "environment-loss fixture starts");
        auto observed = ValidEnvironment();
        item.mutate(observed);
        Check(!capture.validateEnvironment(observed), item.name);
        const auto json = capture.resultJson();
        Check(json.find("\"reason\":\"environment_lost\"") != std::string::npos &&
              json.find("\"stage\":\"frame\"") != std::string::npos &&
              json.find("\"environment_failure\":{") != std::string::npos,
              "environment loss carries a nested diagnostic");
        Check(json.find(std::string("\"failed_fields\":[\"") + item.failedField + "\"]") !=
                  std::string::npos, "single invariant identifies its exact failed field");
        Check(json.find("\"scenario\":\"idle\",\"phase\":\"warmup\"") != std::string::npos &&
              json.find("\"warmup_progress\":0,\"sample_count\":0,\"capture_frame_index\":1") !=
                  std::string::npos, "failure records scenario and capture progress");
        const auto observedAt = json.find("\"observed\":{");
        Check(json.find("\"expected\":{\"window_visible\":true,\"window_focused\":true,"
                        "\"window_iconic\":false,\"client_width\":1280,\"client_height\":720,"
                        "\"dpi_scale\":1,\"opengl\":true,\"vsync\":true,\"audio_device\":true}") !=
                  std::string::npos && observedAt != std::string::npos &&
              json.find(item.observedValue, observedAt) != std::string::npos,
              "failure records exact expected and observed environments");
        Check(json.find("\"scenarios\"") == std::string::npos &&
              json.find("\"value_ms\"") == std::string::npos &&
              json.find("\"budget_ms\"") == std::string::npos,
              "environment loss omits measured fields");
    }
    FrameBudgetDiagnostics invalidValues;
    Check(invalidValues.startCapture({}, ValidEnvironment()) &&
          invalidValues.beginScenario(FrameScenario::Idle), "invalid-value fixture starts");
    auto invalid = ValidEnvironment();
    invalid.client_width = 0; invalid.client_height = 0; invalid.dpi_scale = 0.0;
    Check(!invalidValues.validateEnvironment(invalid), "invalid dimensions fail");
    const auto invalidJson = invalidValues.resultJson();
    Check(invalidJson.find("\"failed_fields\":[\"client_width_invalid\",\"client_width_changed\","
                           "\"client_height_invalid\",\"client_height_changed\","
                           "\"dpi_scale_invalid\",\"dpi_scale_changed\"]") != std::string::npos,
          "invalid values are distinguished from start-value changes in deterministic order");
    auto later = ValidEnvironment(); later.window_visible = false;
    Check(!invalidValues.validateEnvironment(later) && invalidValues.resultJson() == invalidJson,
          "the first environment failure remains latched");

    FrameBudgetDiagnostics progressed;
    Check(progressed.startCapture({}, ValidEnvironment()) && progressed.beginScenario(FrameScenario::Idle),
          "progress fixture starts");
    for (std::size_t i = 0; i < 60; ++i) {
        Check(progressed.validateEnvironment(ValidEnvironment()), "warmup environment remains valid");
        progressed.recordFrame(ValidTiming(), {});
    }
    Check(progressed.validateEnvironment(ValidEnvironment()), "first sampling environment remains valid");
    progressed.recordFrame(ValidTiming(), {});
    auto lostDuringSampling = ValidEnvironment(); lostDuringSampling.vsync = false;
    Check(!progressed.validateEnvironment(lostDuringSampling), "sampling loss fails capture");
    const auto progressedJson = progressed.resultJson();
    Check(progressedJson.find("\"scenario\":\"idle\",\"phase\":\"sampling\"") != std::string::npos &&
          progressedJson.find("\"warmup_progress\":60,\"sample_count\":1,\"capture_frame_index\":62") !=
              std::string::npos, "sampling failure reports exact progress and total frame index");
}
void TestPhaseFailureAndAbortJson() {
    using Clock = FrameBudgetDiagnostics::Clock;
    FrameBudgetDiagnostics running;
    Check(running.startCapture({}, ValidEnvironment()) && running.beginScenario(FrameScenario::Idle),
          "running fixture starts");
    const std::string inProgress = running.resultJson();
    Check(inProgress.find("\"reason\":\"capture_in_progress\"") != std::string::npos &&
          inProgress.find("\"stage\":\"capture\"") != std::string::npos &&
          inProgress.find("\"scenarios\"") == std::string::npos &&
          inProgress.find("\"value_ms\"") == std::string::npos, "running JSON omits measurements");
    FrameBudgetDiagnostics missingPhase;
    Check(missingPhase.startCapture({}, ValidEnvironment()) &&
          missingPhase.beginScenario(FrameScenario::Idle), "phase fixture starts");
    auto timing = ValidTiming();
    timing.glRendered = false;
    Check(!missingPhase.recordFrame(timing, {}) &&
          missingPhase.state() == FrameCaptureState::Failed, "missing real phase fails capture");
    const std::string failed = missingPhase.resultJson();
    Check(failed.find("\"reason\":\"incomplete_frame\"") != std::string::npos &&
          failed.find("\"stage\":\"frame\"") != std::string::npos,
          "failed JSON carries fixed reason and stage");
    Check(failed.find("\"scenarios\"") == std::string::npos &&
          failed.find("\"value_ms\"") == std::string::npos, "failed JSON omits measurements");
    FrameBudgetDiagnostics aborted;
    Check(aborted.startCapture(Clock::time_point{}, ValidEnvironment()) &&
          aborted.beginScenario(FrameScenario::Idle), "abort fixture starts");
    aborted.abort("window_hidden", "visibility");
    Check(aborted.state() == FrameCaptureState::Failed &&
          aborted.resultJson().find("window_hidden") != std::string::npos,
          "abort clears active capture with fixed reason");
}
void TestInvalidFramesAndOrderingFailCapture() {
    using Clock = FrameBudgetDiagnostics::Clock;
    const auto start = Clock::time_point{};
    FrameBudgetDiagnostics incomplete;
    Check(incomplete.startCapture(start, ValidEnvironment()) && incomplete.beginScenario(FrameScenario::Idle),
          "incomplete-frame fixture starts");
    auto timing = ValidTiming();
    timing.complete = false;
    Check(!incomplete.recordFrame(timing, start) &&
          incomplete.state() == FrameCaptureState::Failed,
          "incomplete timing fails instead of dropping a frame");
    FrameBudgetDiagnostics unordered;
    Check(unordered.startCapture(start, ValidEnvironment()) && unordered.beginScenario(FrameScenario::Idle),
          "unordered fixture starts");
    timing = ValidTiming();
    timing.presentEndMs = timing.presentStartMs - 0.1;
    Check(!unordered.recordFrame(timing, start) &&
          unordered.state() == FrameCaptureState::Failed,
          "out-of-order timing fails capture");
    FrameBudgetDiagnostics wrongOrder;
    Check(wrongOrder.startCapture(start, ValidEnvironment()), "wrong-scenario fixture starts");
    Check(!wrongOrder.beginScenario(FrameScenario::BusyScrollMiniPlayback) &&
          wrongOrder.state() == FrameCaptureState::Failed,
          "busy cannot start before idle");
}
void TestNinetySecondDeadline() {
    using Clock = FrameBudgetDiagnostics::Clock;
    const auto start = Clock::time_point{};
    FrameBudgetDiagnostics capture;
    Check(capture.startCapture(start, ValidEnvironment()) && capture.beginScenario(FrameScenario::Idle),
          "deadline fixture starts");
    Check(!capture.deadlineExceeded(start + std::chrono::seconds(90)),
          "exactly 90 seconds remains allowed");
    const auto late = start + std::chrono::seconds(91);
    Check(capture.deadlineExceeded(late), "later than 90 seconds exceeds deadline");
    Check(!capture.recordFrame(ValidTiming(), late) &&
          capture.state() == FrameCaptureState::Failed,
          "deadline failure rejects the frame");
}
}  // namespace
int main() {
    TestSummaryStatistics();
    TestExactScenarioCapacitiesAndTransitions();
    TestEnvironmentGate();
    TestEnvironmentLossDiagnostics();
    TestPhaseFailureAndAbortJson();
    TestInvalidFramesAndOrderingFailCapture();
    TestNinetySecondDeadline();
    if (failures != 0) return 1;
    std::cout << "PASS: FrameBudgetDiagnostics pure sampling contract\n";
    return 0;
}

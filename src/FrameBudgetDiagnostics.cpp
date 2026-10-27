#include "FrameBudgetDiagnostics.h"

#if AURA_BUILD_DIAGNOSTICS && !AURA_INTERNAL_BETA
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <vector>
namespace aura::diagnostics {
namespace {
constexpr std::size_t kWarmupFrames = 60;
constexpr std::size_t kSampleFrames = 240;
constexpr auto kInternalDeadlineSeconds = 90;
constexpr auto kExternalSubprocessTimeoutSeconds = 120;
constexpr const char* kStartCommand = "start_frame_budget_capture";
constexpr const char* kResultCommand = "frame_budget_result";

bool ValidValue(double value) { return std::isfinite(value) && value >= 0.0; }
bool ValidEnvironment(const FrameBudgetEnvironment& e) {
    return e.window_visible && e.window_focused && !e.window_iconic &&
           e.client_width == 1280 && e.client_height == 720 && e.dpi_scale > 0.0 &&
           e.opengl && e.vsync && e.audio_device;
}
void AppendMetric(std::ostringstream& out, const char* name,
                  const std::array<double, 240>& values, std::size_t count) {
    const auto stats = FrameBudgetDiagnostics::summarize(values.data(), count);
    out << "\"" << name << "\":{\"samples\":[";
    for (std::size_t i = 0; i < count; ++i) out << (i ? "," : "") << values[i];
    out << "],\"median_ms\":" << stats.median_ms << ",\"p95_ms\":" << stats.p95_ms
        << ",\"max_ms\":" << stats.max_ms << "}";
}
void AppendScenario(std::ostringstream& out, const char* name, const FrameSamples& samples) {
    out << "\"" << name << "\":{\"warmup_frames\":60,\"sample_count\":" << samples.count << ",";
    AppendMetric(out, "cpu_work_ms", samples.cpuWorkSamplesMs, samples.count); out << ",";
    AppendMetric(out, "present_block_ms", samples.presentBlockSamplesMs, samples.count); out << ",";
    AppendMetric(out, "frame_interval_ms", samples.frameIntervalSamplesMs, samples.count);
    out << ",\"render_ui_frames\":" << samples.renderUiFrames
        << ",\"gl_render_frames\":" << samples.glRenderFrames
        << ",\"present_frames\":" << samples.presentFrames << "}";
}
void AppendEnvironment(std::ostringstream& out, const FrameBudgetEnvironment& environment) {
    out << "{\"window_visible\":" << environment.window_visible
        << ",\"window_focused\":" << environment.window_focused
        << ",\"window_iconic\":" << environment.window_iconic
        << ",\"client_width\":" << environment.client_width
        << ",\"client_height\":" << environment.client_height
        << ",\"dpi_scale\":" << environment.dpi_scale
        << ",\"opengl\":" << environment.opengl << ",\"vsync\":" << environment.vsync
        << ",\"audio_device\":" << environment.audio_device << "}";
}
}
std::size_t FrameBudgetDiagnostics::warmupFrameCount() { return kWarmupFrames; }
std::size_t FrameBudgetDiagnostics::sampleFrameCount() { return kSampleFrames; }
const char* FrameBudgetDiagnostics::scenarioName(FrameScenario scenario) {
    return scenario == FrameScenario::Idle ? "idle" : "busy_scroll_mini_playback";
}
double FrameBudgetDiagnostics::nearestRank(const double* values, std::size_t count,
                                           double percentile) {
    if (!values || count == 0 || percentile <= 0.0 || percentile > 1.0)
        return std::numeric_limits<double>::quiet_NaN();
    std::vector<double> sorted(values, values + count);
    if (!std::all_of(sorted.begin(), sorted.end(), ValidValue))
        return std::numeric_limits<double>::quiet_NaN();
    std::sort(sorted.begin(), sorted.end());
    const auto rank = static_cast<std::size_t>(std::ceil(percentile * count));
    return sorted[(std::max)(std::size_t{1}, rank) - 1];
}
FrameStats FrameBudgetDiagnostics::summarize(const double* values, std::size_t count) {
    FrameStats out;
    if (!values || count == 0) return out;
    std::vector<double> sorted(values, values + count);
    if (!std::all_of(sorted.begin(), sorted.end(), ValidValue)) return out;
    std::sort(sorted.begin(), sorted.end());
    const auto middle = count / 2;
    out.valid = true;
    out.median_ms = count % 2 ? sorted[middle] : (sorted[middle - 1] + sorted[middle]) * 0.5;
    out.p95_ms = nearestRank(sorted.data(), count, 0.95);
    out.max_ms = sorted.back();
    return out;
}
FrameBudgetDiagnostics& FrameBudgetDiagnostics::shared() { static FrameBudgetDiagnostics value; return value; }
const char* FrameBudgetDiagnostics::startCommand() { return kStartCommand; }
const char* FrameBudgetDiagnostics::resultCommand() { return kResultCommand; }
bool FrameBudgetDiagnostics::startCapture(Clock::time_point now, const FrameBudgetEnvironment& environment) {
    if (state_ != FrameCaptureState::Ready) return false;
    environmentFailureLatched_ = false; environmentFailedFieldCount_ = 0; captureFrameIndex_ = 0;
    environment_ = environment;
    if (!ValidEnvironment(environment)) {
        environmentGate_ = true; return fail("environment_invariant", "preflight");
    }
    started_ = now;
    state_ = FrameCaptureState::AwaitingIdle;
    return true;
}
bool FrameBudgetDiagnostics::fail(const char* reason, const char* stage) {
    failureReason_ = reason; failureStage_ = stage; state_ = FrameCaptureState::Failed;
    frameStarted_ = cpuStarted_ = presentStarted_ = false; pending_ = {}; return false;
}
void FrameBudgetDiagnostics::abort(const char* reason, const char* stage) {
    if (active()) fail(reason, stage);
}
bool FrameBudgetDiagnostics::beginScenario(FrameScenario scenario) {
    if (state_ == FrameCaptureState::AwaitingIdle && scenario == FrameScenario::Idle)
        state_ = FrameCaptureState::IdleWarmup;
    else if (state_ == FrameCaptureState::AwaitingBusy && scenario == FrameScenario::BusyScrollMiniPlayback)
        state_ = FrameCaptureState::BusyWarmup;
    else return fail("scenario_order", "scenario_transition");
    warmupCount_ = 0;
    return true;
}
bool FrameBudgetDiagnostics::deadlineExceeded(Clock::time_point now) const {
    return active() && now - started_ > std::chrono::seconds(kInternalDeadlineSeconds);
}
bool FrameBudgetDiagnostics::active() const {
    return state_ != FrameCaptureState::Ready && state_ != FrameCaptureState::Disabled &&
           state_ != FrameCaptureState::Complete && state_ != FrameCaptureState::Failed;
}
bool FrameBudgetDiagnostics::isCaptureActive() { return shared().active(); }
bool FrameBudgetDiagnostics::validateEnvironment(const FrameBudgetEnvironment& environment) {
    if (!active()) return false;
    ++captureFrameIndex_;
    if (ValidEnvironment(environment) && environment.client_width == environment_.client_width &&
        environment.client_height == environment_.client_height && environment.dpi_scale == environment_.dpi_scale)
        return true;
    if (!environmentFailureLatched_) {
        environmentFailureLatched_ = true; failedEnvironment_ = environment;
        auto add = [&](bool failed, const char* name) {
            if (failed) environmentFailedFields_[environmentFailedFieldCount_++] = name;
        };
        add(!environment.window_visible, "window_visible");
        add(!environment.window_focused, "window_focused");
        add(environment.window_iconic, "window_iconic");
        add(environment.client_width <= 0, "client_width_invalid");
        add(environment.client_width != environment_.client_width, "client_width_changed");
        add(environment.client_height <= 0, "client_height_invalid");
        add(environment.client_height != environment_.client_height, "client_height_changed");
        add(!std::isfinite(environment.dpi_scale) || environment.dpi_scale <= 0.0, "dpi_scale_invalid");
        add(environment.dpi_scale != environment_.dpi_scale, "dpi_scale_changed");
        add(!environment.opengl, "opengl"); add(!environment.vsync, "vsync");
        add(!environment.audio_device, "audio_device");
        const bool busy = state_ == FrameCaptureState::AwaitingBusy ||
            state_ == FrameCaptureState::BusyWarmup || state_ == FrameCaptureState::BusyCapture;
        const bool warmup = state_ == FrameCaptureState::IdleWarmup || state_ == FrameCaptureState::BusyWarmup;
        const bool sampling = state_ == FrameCaptureState::IdleCapture || state_ == FrameCaptureState::BusyCapture;
        environmentFailureScenario_ = busy ? scenarioName(FrameScenario::BusyScrollMiniPlayback) :
                                             scenarioName(FrameScenario::Idle);
        environmentFailurePhase_ = warmup ? "warmup" : sampling ? "sampling" : "awaiting";
        environmentFailureWarmupProgress_ = warmup ? warmupCount_ : sampling ? kWarmupFrames : 0;
        environmentFailureSampleCount_ = busy ? result_.busySamples.count : result_.idleSamples.count;
        environmentFailureFrameIndex_ = captureFrameIndex_;
    }
    return fail("environment_lost", "frame");
}
void FrameBudgetDiagnostics::observeBusyFrame(int scroll, bool playing, bool throttled, int visibleRows) {
    if (state_ != FrameCaptureState::BusyCapture) return;
    if (!playing || throttled || visibleRows <= 0) {
        if (throttled) ++busyIntentionalThrottleFrames_;
        fail(!playing ? "playback_lost" : throttled ? "intentional_throttle" : "visible_rows_missing",
             "busy_workload"); return;
    }
    ++playbackFramesObserved_;
    bool seen = false;
    for (int i = 0; i < distinctScrollPositions_; ++i) seen |= scrollPositions_[i] == scroll;
    if (!seen && distinctScrollPositions_ < int(scrollPositions_.size()))
        scrollPositions_[distinctScrollPositions_++] = scroll;
}
void FrameBudgetDiagnostics::observeMiniActivity(int requests, int results) {
    if (state_ == FrameCaptureState::BusyCapture) {
        miniRequestsSubmitted_ += (std::max)(0, requests);
        miniResultsConsumed_ += (std::max)(0, results);
    }
}
bool FrameBudgetDiagnostics::recordFrame(const FrameTiming& t, Clock::time_point now) {
    if (deadlineExceeded(now)) return fail("deadline_exceeded", "frame");
    if (!t.complete || !ValidValue(t.cpuStartMs) || !ValidValue(t.cpuEndMs) ||
        !ValidValue(t.presentStartMs) || !ValidValue(t.presentEndMs) ||
        !ValidValue(t.frameIntervalMs) || t.cpuEndMs < t.cpuStartMs ||
        t.presentStartMs < t.cpuEndMs || t.presentEndMs < t.presentStartMs ||
        !t.renderUi || !t.glRendered || !t.presented) return fail("incomplete_frame", "frame");
    const bool idle = state_ == FrameCaptureState::IdleWarmup || state_ == FrameCaptureState::IdleCapture;
    const bool busy = state_ == FrameCaptureState::BusyWarmup || state_ == FrameCaptureState::BusyCapture;
    if (!idle && !busy) return false;
    if (state_ == FrameCaptureState::IdleWarmup || state_ == FrameCaptureState::BusyWarmup) {
        if (++warmupCount_ == kWarmupFrames)
            state_ = idle ? FrameCaptureState::IdleCapture : FrameCaptureState::BusyCapture;
        return true;
    }
    auto& samples = idle ? result_.idleSamples : result_.busySamples;
    if (samples.count >= kSampleFrames) return false;
    const auto index = samples.count++;
    samples.cpuWorkSamplesMs[index] = t.cpuEndMs - t.cpuStartMs;
    samples.presentBlockSamplesMs[index] = t.presentEndMs - t.presentStartMs;
    samples.frameIntervalSamplesMs[index] = t.frameIntervalMs;
    ++samples.renderUiFrames; ++samples.glRenderFrames; ++samples.presentFrames;
    if (samples.count == kSampleFrames)
        state_ = idle ? FrameCaptureState::AwaitingBusy : FrameCaptureState::Complete;
    return true;
}
void FrameBudgetDiagnostics::beginFrame() {
    auto& s = shared(); if (!s.active()) return;
    const auto now = Clock::now();
    if (s.deadlineExceeded(now)) { s.fail("deadline_exceeded", "frame"); return; }
    if (s.frameStarted_) { s.fail("frame_reentered", "frame"); return; }
    s.pending_ = {}; s.pending_.frameIntervalMs = s.lastFrameStart_ == Clock::time_point{} ? 0.0 :
        std::chrono::duration<double, std::milli>(now - s.lastFrameStart_).count();
    s.lastFrameStart_ = now; s.frameStarted_ = true; s.cpuStarted_ = s.presentStarted_ = false;
}
void FrameBudgetDiagnostics::beginCpuWork() {
    auto& s = shared(); if (s.active() && s.frameStarted_) { s.cpuStart_ = Clock::now(); s.cpuStarted_ = true; }
}
void FrameBudgetDiagnostics::endCpuWork() {
    auto& s = shared(); if (s.active() && s.cpuStarted_)
        s.pending_.cpuEndMs = std::chrono::duration<double, std::milli>(Clock::now() - s.cpuStart_).count();
}
void FrameBudgetDiagnostics::beginPresent() {
    auto& s = shared(); if (s.active() && s.cpuStarted_) { s.presentStart_ = Clock::now(); s.presentStarted_ = true; }
}
void FrameBudgetDiagnostics::endPresent() {
    auto& s = shared(); if (s.active() && s.presentStarted_)
        s.pending_.presentEndMs = std::chrono::duration<double, std::milli>(Clock::now() - s.presentStart_).count();
}
void FrameBudgetDiagnostics::markRenderUi() { auto& s = shared(); if (s.active()) s.pending_.renderUi = true; }
void FrameBudgetDiagnostics::markGlRender() { auto& s = shared(); if (s.active()) s.pending_.glRendered = true; }
void FrameBudgetDiagnostics::markPresent() { auto& s = shared(); if (s.active()) s.pending_.presented = true; }
void FrameBudgetDiagnostics::endFrame() {
    auto& s = shared(); if (!s.active() || !s.frameStarted_) return;
    s.pending_.cpuStartMs = 0.0; s.pending_.presentStartMs = s.pending_.cpuEndMs;
    s.pending_.presentEndMs += s.pending_.presentStartMs;
    s.pending_.complete = s.cpuStarted_ && s.presentStarted_;
    s.frameStarted_ = false; s.recordFrame(s.pending_, Clock::now());
}
std::string FrameBudgetDiagnostics::resultJson() const {
    const auto cpu = summarize(result_.busySamples.cpuWorkSamplesMs.data(), result_.busySamples.count);
    const bool phasesComplete = result_.idleSamples.renderUiFrames == kSampleFrames &&
        result_.idleSamples.glRenderFrames == kSampleFrames && result_.idleSamples.presentFrames == kSampleFrames &&
        result_.busySamples.renderUiFrames == kSampleFrames &&
        result_.busySamples.glRenderFrames == kSampleFrames && result_.busySamples.presentFrames == kSampleFrames;
    const bool authentic = state_ == FrameCaptureState::Complete && phasesComplete && distinctScrollPositions_ >= 120 &&
        playbackFramesObserved_ == int(kSampleFrames) && miniRequestsSubmitted_ > 0 &&
        miniResultsConsumed_ > 0 && busyIntentionalThrottleFrames_ == 0 && cpu.valid;
    const char* status = environmentGate_ ? "environment_gate" : authentic ? "measured" : active() ? "running" : "failed";
    std::ostringstream out; out << std::boolalpha << std::setprecision(9)
        << "{\"ok\":" << (authentic ? "true" : "false")
        << ",\"cmd\":\"" << kResultCommand << "\",\"schema\":1,\"status\":\"" << status
        << "\",\"metric\":\"main_thread_frame_budget\",\"environment\":{"
        << "\"window_visible\":" << environment_.window_visible << ",\"window_focused\":" << environment_.window_focused
        << ",\"window_iconic\":" << environment_.window_iconic << ",\"client_width\":" << environment_.client_width
        << ",\"client_height\":" << environment_.client_height << ",\"dpi_scale\":" << environment_.dpi_scale
        << ",\"opengl\":" << environment_.opengl << ",\"vsync\":" << environment_.vsync
        << ",\"audio_device\":" << environment_.audio_device << "}";
    if (!authentic) {
        const char* reason = active() ? "capture_in_progress" :
            state_ == FrameCaptureState::Complete ? "authenticity_incomplete" : failureReason_;
        const char* stage = active() ? "capture" :
            state_ == FrameCaptureState::Complete ? "result" : failureStage_;
        out << ",\"reason\":\"" << reason << "\",\"stage\":\"" << stage << "\"";
        if (environmentFailureLatched_) {
            out << ",\"environment_failure\":{\"expected\":";
            AppendEnvironment(out, environment_); out << ",\"observed\":";
            AppendEnvironment(out, failedEnvironment_); out << ",\"failed_fields\":[";
            for (std::size_t i = 0; i < environmentFailedFieldCount_; ++i)
                out << (i ? ",\"" : "\"") << environmentFailedFields_[i] << "\"";
            out << "],\"scenario\":\"" << environmentFailureScenario_
                << "\",\"phase\":\"" << environmentFailurePhase_
                << "\",\"warmup_progress\":" << environmentFailureWarmupProgress_
                << ",\"sample_count\":" << environmentFailureSampleCount_
                << ",\"capture_frame_index\":" << environmentFailureFrameIndex_ << "}";
        }
        out << "}";
        return out.str();
    }
    out << ",\"budget_ms\":16,\"scenarios\":{";
    AppendScenario(out, "idle", result_.idleSamples); out << ",";
    AppendScenario(out, "busy_scroll_mini_playback", result_.busySamples);
    out << "},\"distinctScrollPositions\":" << distinctScrollPositions_
        << ",\"playbackFramesObserved\":" << playbackFramesObserved_
        << ",\"miniRequestsSubmitted\":" << miniRequestsSubmitted_
        << ",\"miniResultsConsumed\":" << miniResultsConsumed_
        << ",\"busyIntentionalThrottleFrames\":" << busyIntentionalThrottleFrames_
        << ",\"internal_deadline_seconds\":" << kInternalDeadlineSeconds
        << ",\"external_timeout_seconds\":" << kExternalSubprocessTimeoutSeconds
        << ",\"value_ms\":" << cpu.p95_ms << "}";
    return out.str();
}
}  // namespace aura::diagnostics
#endif

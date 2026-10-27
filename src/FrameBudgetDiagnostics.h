#pragma once
#include "BuildConfig.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <limits>
#include <string>

// @stage7-contract
// API contract: FrameBudgetDiagnostics is a diagnostics-only capture state
// machine owned by the UI/render thread. Frame timing inputs are borrowed for
// each call, result snapshots and JSON are returned by value, and environment
// validation must pass before capture begins. Disabled builds preserve the
// no-op contract; no worker or caller memory is retained across shutdown.
namespace aura::diagnostics {
enum class FrameScenario { Idle, BusyScrollMiniPlayback };
enum class FrameCaptureState {
    Disabled, Ready, AwaitingIdle, IdleWarmup, IdleCapture, AwaitingBusy,
    BusyWarmup, BusyCapture, Complete, Failed
};
struct FrameTiming {
    double cpuStartMs, cpuEndMs, presentStartMs, presentEndMs, frameIntervalMs;
    bool complete, renderUi, glRendered, presented;
};
struct FrameStats {
    bool valid = false;
    double median_ms = 0.0, p95_ms = 0.0, max_ms = 0.0;
};
struct FrameSamples {
    std::array<double, 240> cpuWorkSamplesMs{};
    std::array<double, 240> presentBlockSamplesMs{};
    std::array<double, 240> frameIntervalSamplesMs{};
    std::size_t count = 0;
    std::size_t renderUiFrames = 0, glRenderFrames = 0, presentFrames = 0;
};
struct FrameCaptureResult { FrameSamples idleSamples; FrameSamples busySamples; };
struct FrameBudgetEnvironment {
    bool window_visible, window_focused, window_iconic;
    int client_width, client_height;
    double dpi_scale;
    bool opengl, vsync, audio_device;
};
#if AURA_BUILD_DIAGNOSTICS && !AURA_INTERNAL_BETA
class FrameBudgetDiagnostics {
public:
    using Clock = std::chrono::steady_clock;
    static std::size_t warmupFrameCount();
    static std::size_t sampleFrameCount();
    static const char* scenarioName(FrameScenario scenario);
    static double nearestRank(const double* values, std::size_t count, double percentile);
    static FrameStats summarize(const double* values, std::size_t count);
    static FrameBudgetDiagnostics& shared();
    static const char* startCommand();
    static const char* resultCommand();
    static bool isCaptureActive();
    static void beginFrame();
    static void beginCpuWork();
    static void endCpuWork();
    static void beginPresent();
    static void endPresent();
    static void markRenderUi();
    static void markGlRender();
    static void markPresent();
    static void endFrame();
    bool startCapture(Clock::time_point now, const FrameBudgetEnvironment& environment);
    bool beginScenario(FrameScenario scenario);
    bool recordFrame(const FrameTiming& timing, Clock::time_point now);
    bool validateEnvironment(const FrameBudgetEnvironment& environment);
    void observeBusyFrame(int scrollPosition, bool playing, bool throttled, int visibleRows);
    void observeMiniActivity(int requests, int results);
    void abort(const char* reason, const char* stage);
    bool deadlineExceeded(Clock::time_point now) const;
    FrameCaptureState state() const { return state_; }
    std::size_t busySampleCount() const { return result_.busySamples.count; }
    FrameCaptureResult result() const { return result_; }
    std::string resultJson() const;
private:
    bool fail(const char* reason, const char* stage);
    bool active() const;
    FrameCaptureState state_ = FrameCaptureState::Ready;
    Clock::time_point started_{};
    std::size_t warmupCount_ = 0;
    FrameCaptureResult result_{};
    FrameBudgetEnvironment environment_{};
    FrameBudgetEnvironment failedEnvironment_{};
    std::array<const char*, 12> environmentFailedFields_{};
    std::size_t environmentFailedFieldCount_ = 0, captureFrameIndex_ = 0;
    std::size_t environmentFailureWarmupProgress_ = 0, environmentFailureSampleCount_ = 0;
    std::size_t environmentFailureFrameIndex_ = 0;
    const char* environmentFailureScenario_ = "idle";
    const char* environmentFailurePhase_ = "awaiting";
    bool environmentFailureLatched_ = false;
    std::array<int, 240> scrollPositions_{};
    int distinctScrollPositions_ = 0, playbackFramesObserved_ = 0;
    int miniRequestsSubmitted_ = 0, miniResultsConsumed_ = 0, busyIntentionalThrottleFrames_ = 0;
    Clock::time_point lastFrameStart_{}, cpuStart_{}, presentStart_{};
    FrameTiming pending_{};
    bool frameStarted_ = false, cpuStarted_ = false, presentStarted_ = false, environmentGate_ = false;
    const char* failureReason_ = "capture_not_started";
    const char* failureStage_ = "control";
};
#else
class FrameBudgetDiagnostics {
public:
    using Clock = std::chrono::steady_clock;
    static constexpr std::size_t warmupFrameCount() { return 60; }
    static constexpr std::size_t sampleFrameCount() { return 240; }
    static constexpr const char* scenarioName(FrameScenario) { return "disabled"; }
    static double nearestRank(const double*, std::size_t, double) { return std::numeric_limits<double>::quiet_NaN(); }
    static FrameStats summarize(const double*, std::size_t) { return {}; }
    static inline bool isCaptureActive() { return false; }
    static inline void beginFrame() {}
    static inline void beginCpuWork() {}
    static inline void endCpuWork() {}
    static inline void beginPresent() {}
    static inline void endPresent() {}
    static inline void markRenderUi() {}
    static inline void markGlRender() {}
    static inline void markPresent() {}
    static inline void endFrame() {}
    inline bool startCapture(Clock::time_point, const FrameBudgetEnvironment&) { return false; }
    inline bool beginScenario(FrameScenario) { return false; }
    inline bool recordFrame(const FrameTiming&, Clock::time_point) { return false; }
    inline bool validateEnvironment(const FrameBudgetEnvironment&) { return false; }
    inline void observeBusyFrame(int, bool, bool, int) {}
    inline void observeMiniActivity(int, int) {}
    inline void abort(const char*, const char*) {}
    inline bool deadlineExceeded(Clock::time_point) const { return false; }
    inline FrameCaptureState state() const { return FrameCaptureState::Disabled; }
    inline std::size_t busySampleCount() const { return 0; }
    inline FrameCaptureResult result() const { return {}; }
    inline std::string resultJson() const { return "{}"; }
};
#endif
}  // namespace aura::diagnostics

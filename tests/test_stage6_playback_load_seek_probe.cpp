#include "AudioEngine.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#endif

namespace {
namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
constexpr uint32_t kRate = 48000;
constexpr uint32_t kChannels = 2;
constexpr uint32_t kFrames = 480000;
constexpr double kDuration = 10.0;
constexpr int kLoads = 20;
constexpr int kSeeks = 100;
constexpr int kPollIntervalMs = 1;
constexpr int kPerLoadTimeoutSeconds = 5;
constexpr int kProbeDeadlineSeconds = 45;

class ScopedTimerResolution {
public:
    ScopedTimerResolution() {
#ifdef _WIN32
        active_ = timeBeginPeriod(kPollIntervalMs) == TIMERR_NOERROR;
#endif
    }
    ~ScopedTimerResolution() {
#ifdef _WIN32
        if (active_) timeEndPeriod(kPollIntervalMs);
#endif
    }
    bool Succeeded() const noexcept { return active_; }

private:
    bool active_ = true;
};

struct Stats { double median = 0.0, p95 = 0.0, max = 0.0; };
struct ProbeResult { int code = 1; std::string json; };

bool finiteNonNegative(double value) { return std::isfinite(value) && value >= 0.0; }

Stats summarize(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    const auto at = [&](double fraction) {
        const auto index = static_cast<std::size_t>(std::ceil(fraction * values.size())) - 1U;
        return values[(std::min)(index, values.size() - 1U)];
    };
    return {at(0.50), at(0.95), values.back()};
}

std::string statusJson(const char* status, const char* stage, const char* reason) {
    std::ostringstream out;
    out << "{\"schema\":1,\"status\":\"" << status
        << "\",\"stage\":\"" << stage << "\",\"reason\":\"" << reason << "\"}";
    return out.str();
}

bool writeFixture(const fs::path& path) {
    constexpr uint16_t bits = 16;
    constexpr uint32_t dataBytes = kFrames * kChannels * (bits / 8U);
    constexpr uint32_t riffSize = 4 + (8 + 16) + (8 + dataBytes);
    constexpr double pi2 = 6.28318530717958647692;
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    const auto put32 = [&](uint32_t value) {
        const char b[4] = {char(value), char(value >> 8), char(value >> 16), char(value >> 24)};
        out.write(b, 4);
    };
    const auto put16 = [&](uint16_t value) {
        const char b[2] = {char(value), char(value >> 8)};
        out.write(b, 2);
    };
    out.write("RIFF", 4); put32(riffSize); out.write("WAVEfmt ", 8);
    put32(16); put16(1); put16(kChannels); put32(kRate);
    put32(kRate * kChannels * (bits / 8U)); put16(kChannels * (bits / 8U)); put16(bits);
    out.write("data", 4); put32(dataBytes);
    std::vector<int16_t> samples(static_cast<std::size_t>(kFrames) * kChannels);
    for (uint32_t frame = 0; frame < kFrames; ++frame) {
        const double time = static_cast<double>(frame) / kRate;
        samples[static_cast<std::size_t>(frame) * kChannels] =
            static_cast<int16_t>(std::sin(pi2 * 440.0 * time) * 18000.0);
        samples[static_cast<std::size_t>(frame) * kChannels + 1U] =
            static_cast<int16_t>(std::sin(pi2 * 660.0 * time) * 16000.0);
    }
    out.write(reinterpret_cast<const char*>(samples.data()), static_cast<std::streamsize>(dataBytes));
    return out.good();
}

ProbeResult runProbe() {
    ScopedTimerResolution timerResolution;
    if (!timerResolution.Succeeded()) {
        return {2, statusJson("environment_gate", "timer_resolution",
                             "1 ms timer resolution is unavailable")};
    }
    fs::path root;
    try {
        root = fs::temp_directory_path() /
               ("aris-stage6-playback-" + std::to_string(Clock::now().time_since_epoch().count()));
        fs::create_directories(root);
    } catch (...) {
        return {2, statusJson("environment_gate", "fixture", "temporary fixture directory is unavailable")};
    }
    const fs::path fixture = root / "fixture.wav";
    if (!writeFixture(fixture)) {
        std::error_code ec; fs::remove_all(root, ec);
        return {2, statusJson("environment_gate", "fixture", "fixture file is unavailable")};
    }

    AudioEngine engine;
    const auto cleanup = [&]() {
        engine.shutdown();
        std::error_code ec; fs::remove_all(root, ec); return ec;
    };
    const auto failed = [&](const char* stage, const char* reason) {
        cleanup(); return ProbeResult{1, statusJson("failed", stage, reason)};
    };
    if (!engine.initialize(10)) {
        cleanup(); return {2, statusJson("environment_gate", "audio_initialize", "default audio device initialization failed")};
    }
    engine.setVolume(0.0f); engine.setFadeInOutEnabled(false); engine.setFadeInOutMs(0);

    const auto pump = [&](std::chrono::seconds timeout) {
        const auto deadline = Clock::now() + timeout;
        while (Clock::now() < deadline) {
            if (engine.pumpLoadFileIfPending()) return true;
            if (!engine.isLoadFilePending() && !engine.isLoaded()) return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(kPollIntervalMs));
        }
        return false;
    };
    const auto loadedFixture = [&]() {
        const AudioFileInfo& info = engine.getFileInfo();
        return engine.isLoaded() && engine.getLoadedFilePath() == fixture.string() &&
               info.sampleRate == kRate && info.channels == kChannels &&
               info.totalFrames == kFrames && std::abs(info.durationSeconds - kDuration) < 0.001 &&
               info.format == "WAV";
    };

    engine.loadFileAsync(fixture.string(), false);
    if (!pump(std::chrono::seconds(kPerLoadTimeoutSeconds)) || !loadedFixture())
        return failed("warmup", "warm-up load did not satisfy fixture invariants");
    engine.seekToPosition(0.5f);
    if (std::abs(engine.getPlaybackPosition() - 0.5f) > 0.0001f)
        return failed("warmup", "warm-up seek cursor invariant failed");
    engine.unload();

    const auto startedProbe = Clock::now();
    std::vector<double> loadTimes; loadTimes.reserve(kLoads);
    for (int sample = 0; sample < kLoads; ++sample) {
        if (Clock::now() - startedProbe > std::chrono::seconds(kProbeDeadlineSeconds))
            return failed("load", "probe deadline exceeded");
        engine.unload();
        const auto started = Clock::now(); engine.loadFileAsync(fixture.string(), false);
        if (!pump(std::chrono::seconds(kPerLoadTimeoutSeconds)) || !loadedFixture())
            return failed("load", "load-to-ready invariant or timeout failed");
        const double elapsed = std::chrono::duration<double, std::milli>(Clock::now() - started).count();
        if (!finiteNonNegative(elapsed)) return failed("load", "non-finite load timing");
        loadTimes.push_back(elapsed);
    }

    constexpr double positions[] = {0.10, 0.90, 0.25, 0.75, 0.40, 0.60, 0.05, 0.95, 0.33, 0.67};
    std::vector<double> seekTimes; seekTimes.reserve(kSeeks); double maxError = 0.0;
    for (int sample = 0; sample < kSeeks; ++sample) {
        if (Clock::now() - startedProbe > std::chrono::seconds(kProbeDeadlineSeconds))
            return failed("seek", "probe deadline exceeded");
        const double target = positions[sample % 10];
        const auto started = Clock::now(); engine.seekToPosition(static_cast<float>(target));
        const double elapsed = std::chrono::duration<double, std::milli>(Clock::now() - started).count();
        const double error = std::abs(static_cast<double>(engine.getPlaybackPosition()) - target);
        maxError = (std::max)(maxError, error);
        if (!finiteNonNegative(elapsed) || !std::isfinite(error) || error > 0.0001)
            return failed("seek", "seek timing or cursor invariant failed");
        seekTimes.push_back(elapsed);
    }

    const Stats load = summarize(loadTimes), seek = summarize(seekTimes);
    const double valueMs = (std::max)(load.p95, seek.p95);
    const bool invalidStats =
        !finiteNonNegative(load.median) || !finiteNonNegative(load.p95) || !finiteNonNegative(load.max) ||
        !finiteNonNegative(seek.median) || !finiteNonNegative(seek.p95) || !finiteNonNegative(seek.max) ||
        !finiteNonNegative(maxError) || !finiteNonNegative(valueMs);
    const bool cleanupFailed = static_cast<bool>(cleanup());
    if (invalidStats || cleanupFailed)
        return {1, statusJson("failed", "cleanup", "timing or fixture cleanup invariant failed")};

    std::ostringstream json; json << std::fixed << std::setprecision(6);
    json << "{\"schema\":1,\"status\":\"measured\",\"fixture\":{";
    json << "\"format\":\"pcm_s16_wav\",\"sample_rate\":" << kRate
         << ",\"channels\":" << kChannels << ",\"duration_seconds\":" << kDuration
         << ",\"total_frames\":" << kFrames << "},";
    json << "\"load_to_ready\":{\"count\":" << kLoads << ",\"median_ms\":" << load.median
         << ",\"p95_ms\":" << load.p95 << ",\"max_ms\":" << load.max
         << ",\"poll_interval_ms\":" << kPollIntervalMs << "},";
    json << "\"seek_call\":{\"count\":" << kSeeks << ",\"median_ms\":" << seek.median
         << ",\"p95_ms\":" << seek.p95 << ",\"max_ms\":" << seek.max
         << ",\"max_position_error\":" << maxError << "},\"value_ms\":" << valueMs << "}";
    return {0, json.str()};
}
} // namespace

int main() {
    std::ostringstream captured; std::streambuf* old = std::cout.rdbuf(captured.rdbuf());
    ProbeResult result;
    try { result = runProbe(); }
    catch (...) { result = {1, statusJson("failed", "probe", "unexpected probe exception")}; }
    std::cout.rdbuf(old);
    if (result.code != 0 && !captured.str().empty()) std::cerr << captured.str();
    std::cout << result.json << '\n';
    return result.code;
}

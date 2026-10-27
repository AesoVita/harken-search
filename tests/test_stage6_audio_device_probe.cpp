#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

namespace {

constexpr std::uint32_t kTargetCallbacks = 250;

struct ProbeState {
    std::array<std::int64_t, kTargetCallbacks> startNs{};
    std::array<std::int64_t, kTargetCallbacks> executionNs{};
    std::atomic<std::uint32_t> callbackCount{0};
};

static std::int64_t NowNs() noexcept {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

static void AudioCallback(ma_device* device, void* output, const void*, ma_uint32 frameCount) {
    auto* state = static_cast<ProbeState*>(device->pUserData);
    const std::int64_t started = NowNs();
    std::memset(output, 0, static_cast<std::size_t>(frameCount) * sizeof(float) * 2U);
    const std::int64_t finished = NowNs();
    const std::uint32_t index = state->callbackCount.load(std::memory_order_relaxed);
    if (index < kTargetCallbacks) {
        state->startNs[index] = started;
        state->executionNs[index] = finished - started;
        state->callbackCount.store(index + 1, std::memory_order_release);
    }
}

static double Percentile(std::vector<double> values, double percentile) {
    std::sort(values.begin(), values.end());
    const std::size_t index = static_cast<std::size_t>(
        std::ceil(percentile * static_cast<double>(values.size()))) - 1U;
    return values[(std::min)(index, values.size() - 1U)];
}

static int EnvironmentGate(const char* reason, ma_result result) {
    std::cout << "{\"status\":\"environment_gate\",\"reason\":\"" << reason
              << "\",\"miniaudio_result\":" << result << "}\n";
    return 2;
}

} // namespace

int main() {
    ProbeState state;
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate = 48000;
    config.noFixedSizedCallback = MA_TRUE;
    config.dataCallback = AudioCallback;
    config.pUserData = &state;

    ma_device device{};
    ma_result result = ma_device_init(nullptr, &config, &device);
    if (result != MA_SUCCESS)
        return EnvironmentGate("default playback device initialization failed", result);
    result = ma_device_start(&device);
    if (result != MA_SUCCESS) {
        ma_device_uninit(&device);
        return EnvironmentGate("default playback device start failed", result);
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    while (state.callbackCount.load(std::memory_order_acquire) < kTargetCallbacks &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    ma_device_stop(&device);
    const std::uint32_t count = state.callbackCount.load(std::memory_order_acquire);
    const double periodMs = device.sampleRate > 0
        ? 1000.0 * static_cast<double>(device.playback.internalPeriodSizeInFrames) /
              static_cast<double>(device.sampleRate)
        : 0.0;
    ma_device_uninit(&device);
    if (count < kTargetCallbacks)
        return EnvironmentGate("timed out before 250 live callbacks", MA_ERROR);

    std::vector<double> executionMs;
    std::vector<double> jitterMs;
    executionMs.reserve(count);
    jitterMs.reserve(count - 1U);
    for (std::uint32_t index = 0; index < count; ++index) {
        executionMs.push_back(static_cast<double>(state.executionNs[index]) / 1.0e6);
        if (index > 0) {
            const double intervalMs = static_cast<double>(
                state.startNs[index] - state.startNs[index - 1U]) / 1.0e6;
            jitterMs.push_back(std::abs(intervalMs - periodMs));
        }
    }

    std::cout << std::fixed << std::setprecision(6)
              << "{\"status\":\"measured\",\"period_ms\":" << periodMs
              << ",\"callback_execution\":{\"median_ms\":" << Percentile(executionMs, 0.50)
              << ",\"p95_ms\":" << Percentile(executionMs, 0.95)
              << ",\"max_ms\":" << *std::max_element(executionMs.begin(), executionMs.end())
              << "},\"inter_arrival\":{\"p95_jitter_ms\":" << Percentile(jitterMs, 0.95)
              << "},\"callback_count\":" << count << "}\n";
    return 0;
}

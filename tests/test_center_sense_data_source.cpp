#include "CenterSenseDataSource.h"
#include "CenterSenseDsp.h"
#include "platform/audio_data_source.h"
#include "../thirdparty/miniaudio.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <type_traits>
#include <vector>

static_assert(sizeof(CenterSenseDataSource::SourceRef) == sizeof(void*));
static_assert(std::is_trivially_copyable_v<CenterSenseDataSource::SourceRef>);

namespace {
struct MemorySource {
    ma_data_source_base base{};
    float samples[8]{0.1f, -0.2f, 0.3f, -0.4f, 0.5f, -0.6f, 0.7f, -0.8f};
    ma_uint64 cursor = 0;
    ma_bool32 looping = MA_FALSE;
};

MemorySource* Self(ma_data_source* source) {
    return reinterpret_cast<MemorySource*>(
        reinterpret_cast<char*>(source) - offsetof(MemorySource, base));
}

ma_result Read(ma_data_source* source, void* output, ma_uint64 requested,
               ma_uint64* framesRead) {
    auto* memory = Self(source);
    const auto count = std::min(requested, 4 - std::min<ma_uint64>(memory->cursor, 4));
    std::memcpy(output, memory->samples + memory->cursor * 2,
                static_cast<size_t>(count * 2) * sizeof(float));
    memory->cursor += count;
    if (framesRead) *framesRead = count;
    return MA_SUCCESS;
}

ma_result Seek(ma_data_source* source, ma_uint64 frame) {
    if (frame > 4) return MA_INVALID_ARGS;
    Self(source)->cursor = frame;
    return MA_SUCCESS;
}

ma_result Format(ma_data_source*, ma_format* format, ma_uint32* channels,
                 ma_uint32* sampleRate, ma_channel*, size_t) {
    if (format) *format = ma_format_f32;
    if (channels) *channels = 2;
    if (sampleRate) *sampleRate = 48000;
    return MA_SUCCESS;
}

ma_result Cursor(ma_data_source* source, ma_uint64* cursor) {
    *cursor = Self(source)->cursor;
    return MA_SUCCESS;
}
ma_result Length(ma_data_source*, ma_uint64* length) { *length = 4; return MA_SUCCESS; }
ma_result Loop(ma_data_source* source, ma_bool32 looping) {
    Self(source)->looping = looping;
    return MA_SUCCESS;
}

const ma_data_source_vtable kVtable{Read, Seek, Format, Cursor, Length, Loop, 0};

bool Check(bool condition, const char* message) {
    if (condition) return true;
    std::cerr << "FAIL: " << message << '\n';
    return false;
}

bool CheckMagicEQOnPcmBuffer(bool reverseFrames) {
    constexpr std::uint32_t kRate = 48000;
    constexpr std::size_t kFrames = 4800;
    constexpr double kPi = 3.14159265358979323846;
    std::vector<float> samples(kFrames * 2);
    for (std::size_t frame = 0; frame < kFrames; ++frame) {
        const float sample = static_cast<float>(
            0.30 * std::sin(2.0 * kPi * 1000.0 * frame / kRate) +
            0.08 * std::sin(2.0 * kPi * 5000.0 * frame / kRate));
        samples[frame * 2] = sample;
        samples[frame * 2 + 1] = sample;
    }
    if (reverseFrames) {
        for (std::size_t frame = 0; frame < kFrames / 2; ++frame) {
            const std::size_t opposite = kFrames - frame - 1;
            for (std::size_t channel = 0; channel < 2; ++channel)
                std::swap(samples[frame * 2 + channel], samples[opposite * 2 + channel]);
        }
    }
    const std::vector<float> original = samples;
    Platform::AudioDataSource::PcmBuffer buffer;
    if (!Check(Platform::AudioDataSource::InitializePcmBuffer(
                   buffer, 2, kFrames, samples.data()).Succeeded(),
               reverseFrames ? "reversed PCM fixture initializes" : "forward PCM fixture initializes"))
        return false;

    CenterSenseDsp::MagicEQSettingsPublisher publisher;
    CenterSenseDsp::MagicEQSettings settings;
    settings.enabled = true;
    settings.mode = CenterSenseDsp::MagicEQMode::Notch;
    settings.steepness = CenterSenseDsp::MagicEQSteepness::Brickwall;
    settings.centerFreq = 1000.0f;
    settings.halfWidthPercent = 5.0f;
    publisher.store(settings);
    float centerSense = 0.0f;
    CenterSenseDataSource wrapper;
    bool ok = Check(wrapper.init(
        Platform::AudioDataSource::BorrowDataSource(buffer), kRate,
        &centerSense, nullptr, nullptr, nullptr, &publisher),
        reverseFrames ? "reversed PCM connects to EQ source" : "forward PCM connects to EQ source");
    if (ok) {
        auto source = Platform::AudioDataSource::BorrowDataSource(wrapper.getDataSource());
        std::vector<float> enabled(samples.size());
        std::vector<float> disabled(samples.size());
        const auto onRead = Platform::AudioDataSource::ReadPcmFrames(source, enabled.data(), kFrames);
        settings.enabled = false;
        publisher.store(settings);
        const auto seek = Platform::AudioDataSource::SeekPcmFrame(source, 0);
        const auto offRead = Platform::AudioDataSource::ReadPcmFrames(source, disabled.data(), kFrames);
        double differenceEnergy = 0.0;
        double outputEnergy = 0.0;
        double bypassError = 0.0;
        for (std::size_t index = 0; index < samples.size(); ++index) {
            const double difference = enabled[index] - disabled[index];
            differenceEnergy += difference * difference;
            outputEnergy += static_cast<double>(enabled[index]) * enabled[index];
            bypassError = std::max(bypassError, std::abs(double(disabled[index] - samples[index])));
        }
        const bool validReads = onRead.backendCode == 0 && onRead.framesRead == kFrames &&
            seek.Succeeded() && offRead.backendCode == 0 && offRead.framesRead == kFrames;
        ok &= Check(validReads && outputEnergy > 0.0 &&
                        std::sqrt(differenceEnergy / samples.size()) > 0.01,
                    reverseFrames ? "reversed PCM EQ output differs from nonzero bypass"
                                  : "forward PCM EQ output differs from nonzero bypass");
        ok &= Check(validReads && bypassError < 1e-6 && samples == original,
                    reverseFrames ? "reversed PCM bypass preserves source" : "forward PCM bypass preserves source");
    }
    wrapper.uninit();
    Platform::AudioDataSource::UninitializePcmBuffer(buffer);
    return ok;
}
} // namespace

int main() {
    MemorySource memory;
    auto config = ma_data_source_config_init();
    config.vtable = &kVtable;
    if (!Check(ma_data_source_init(&config, &memory.base) == MA_SUCCESS, "fixture init")) return 1;

    float centerSense = 0.0f;
    CenterSenseDataSource wrapper;
    bool ok = Check(wrapper.init(CenterSenseDataSource::SourceRef(
                                     Platform::AudioDataSource::BorrowDataSource(&memory.base)),
                                  48000, &centerSense, nullptr, nullptr, nullptr),
                    "wrapper init");
    auto* source = static_cast<ma_data_source*>(wrapper.getDataSource());
    float output[6]{};
    ma_uint64 read = 0;
    ok &= Check(ma_data_source_read_pcm_frames(source, output, 3, &read) == MA_SUCCESS &&
                read == 3 && std::memcmp(output, memory.samples, sizeof(output)) == 0,
                "direct read preserves samples");
    ok &= Check(ma_data_source_seek_to_pcm_frame(source, 1) == MA_SUCCESS, "seek forwards");
    float sought[2]{};
    ok &= Check(ma_data_source_read_pcm_frames(source, sought, 1, &read) == MA_SUCCESS &&
                read == 1 && sought[0] == memory.samples[2] && sought[1] == memory.samples[3],
                 "seeked direct read preserves borrowed decoder");
    ok &= Check(ma_data_source_set_looping(source, MA_TRUE) == MA_SUCCESS && memory.looping,
                "looping forwards");

    wrapper.uninit();
    ok &= Check(ma_data_source_seek_to_pcm_frame(&memory.base, 0) == MA_SUCCESS,
                "wrapper does not destroy borrowed decoder");
    ma_data_source_uninit(&memory.base);
    ok &= CheckMagicEQOnPcmBuffer(false);
    ok &= CheckMagicEQOnPcmBuffer(true);
    return ok ? 0 : 1;
}

// Regression coverage for F-019-channel: extracting one native channel from a
// multi-channel WAV must preserve the requested channel only.
#include "src/AudioEngine.h"
#include "src/SingleChannelExportWorkflow.h"
#include "src/SingleChannelExportCompletionWorkflow.h"

#include "../thirdparty/miniaudio.h"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static bool writeSixChannelWav(const fs::path& path) {
    constexpr uint32_t sampleRate = 48000;
    constexpr uint32_t frameCount = 9600;
    constexpr uint16_t channels = 6;
    constexpr uint16_t bits = 16;
    const double freqs[channels] = {440.0, 550.0, 660.0, 770.0, 880.0, 990.0};
    constexpr double twoPi = 6.28318530717958647692;

    std::vector<int16_t> interleaved((size_t)frameCount * channels);
    for (uint32_t frame = 0; frame < frameCount; ++frame) {
        const double t = (double)frame / (double)sampleRate;
        for (uint16_t ch = 0; ch < channels; ++ch) {
            interleaved[(size_t)frame * channels + ch] =
                (int16_t)(std::sin(twoPi * freqs[ch] * t) * 18000.0);
        }
    }

    const uint32_t dataBytes = (uint32_t)(interleaved.size() * sizeof(int16_t));
    const uint32_t fmtChunkSize = 16;
    const uint32_t riffSize = 4 + (8 + fmtChunkSize) + (8 + dataBytes);

    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    auto put32 = [&](uint32_t v) {
        char b[4] = {char(v & 0xff), char((v >> 8) & 0xff),
                     char((v >> 16) & 0xff), char((v >> 24) & 0xff)};
        out.write(b, 4);
    };
    auto put16 = [&](uint16_t v) {
        char b[2] = {char(v & 0xff), char((v >> 8) & 0xff)};
        out.write(b, 2);
    };

    out.write("RIFF", 4);
    put32(riffSize);
    out.write("WAVE", 4);
    out.write("fmt ", 4);
    put32(fmtChunkSize);
    put16(1);
    put16(channels);
    put32(sampleRate);
    put32(sampleRate * channels * (bits / 8));
    put16(channels * (bits / 8));
    put16(bits);
    out.write("data", 4);
    put32(dataBytes);
    out.write(reinterpret_cast<const char*>(interleaved.data()), dataBytes);
    return out.good();
}

static double estimateFrequencyHz(const std::vector<float>& mono, uint32_t sampleRate) {
    int positiveCrossings = 0;
    for (size_t i = 1; i < mono.size(); ++i) {
        if (mono[i - 1] <= 0.0f && mono[i] > 0.0f) {
            ++positiveCrossings;
        }
    }
    const double durationSec = (double)mono.size() / (double)sampleRate;
    return durationSec > 0.0 ? (double)positiveCrossings / durationSec : 0.0;
}

static bool decodeMono(const fs::path& path,
                       std::vector<float>& samples,
                       uint32_t& channels,
                       uint32_t& sampleRate) {
    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
    ma_decoder decoder;
#ifdef _WIN32
    ma_result result = ma_decoder_init_file_w(path.c_str(), &config, &decoder);
#else
    ma_result result = ma_decoder_init_file(path.string().c_str(), &config, &decoder);
#endif
    if (result != MA_SUCCESS) return false;
    channels = decoder.outputChannels;
    sampleRate = decoder.outputSampleRate;
    ma_uint64 frames = 0;
    ma_decoder_get_length_in_pcm_frames(&decoder, &frames);
    samples.resize((size_t)frames * channels);
    ma_uint64 framesRead = 0;
    result = ma_decoder_read_pcm_frames(&decoder, samples.data(), frames, &framesRead);
    ma_decoder_uninit(&decoder);
    samples.resize((size_t)framesRead * channels);
    return result == MA_SUCCESS || result == MA_AT_END;
}

int main() {
    int failures = 0;
    auto check = [&](bool cond, const char* label) {
        if (cond) {
            std::cout << "PASS: " << label << "\n";
        } else {
            std::cerr << "FAIL: " << label << "\n";
            ++failures;
        }
    };

    fs::path tempDir = fs::temp_directory_path() / "auro_single_channel_export_test";
    std::error_code ec;
    fs::remove_all(tempDir, ec);
    fs::create_directories(tempDir, ec);

    fs::path input = tempDir / "six_channel.wav";
    fs::path centerOut = tempDir / "center_C.wav";
    fs::path lfeOut = tempDir / "lfe.wav";

    check(writeSixChannelWav(input), "six-channel source WAV created");

    const auto center = aura::audio_export::RunSingleChannelExport({
        input.string(), "center.wav", tempDir.string(), 2, 6});
    check(center.ok, "center channel export succeeds");
    check(center.errorMessage.empty(), "center channel export leaves no error");
    std::string err;
    check(std::filesystem::path(center.outputPath).filename() == centerOut.filename(),
          "center channel export preserves requested destination name");
    check(AudioEngine::exportSingleChannel(input.string(), 2, centerOut.string(), &err),
          "direct center channel export remains available");
    check(AudioEngine::exportSingleChannel(input.string(), 3, lfeOut.string(), &err),
          "LFE channel export succeeds");

    std::vector<float> decoded;
    uint32_t channels = 0;
    uint32_t sampleRate = 0;
    check(decodeMono(centerOut, decoded, channels, sampleRate),
          "center export decodes");
    check(channels == 1, "center export is mono");
    check(sampleRate == 48000, "center export preserves sample rate");
    const double centerHz = estimateFrequencyHz(decoded, sampleRate);
    check(std::abs(centerHz - 660.0) < 8.0,
          "center export contains the center-channel 660 Hz tone");

    decoded.clear();
    channels = 0;
    sampleRate = 0;
    check(decodeMono(lfeOut, decoded, channels, sampleRate),
          "LFE export decodes");
    const double lfeHz = estimateFrequencyHz(decoded, sampleRate);
    check(channels == 1 && std::abs(lfeHz - 770.0) < 8.0,
          "LFE export contains the channel-index-3 770 Hz tone");

    fs::path badOut = tempDir / "bad.wav";
    err.clear();
    check(!AudioEngine::exportSingleChannel(input.string(), 9, badOut.string(), &err),
          "out-of-range channel export fails");
    check(!err.empty(), "out-of-range channel export reports an error");

    std::promise<AppState::ChannelExportJobResult> readyPromise;
    AppState::ChannelExportJobResult readyResult;
    readyResult.ok = true;
    readyResult.outputPath = center.outputPath;
    readyPromise.set_value(readyResult);
    AppState::ChannelExportJob readyJob;
    readyJob.future = readyPromise.get_future();
    readyJob.channelLabel = "C";

    std::promise<AppState::ChannelExportJobResult> failedPromise;
    failedPromise.set_exception(
        std::make_exception_ptr(std::runtime_error("export future failed")));
    AppState::ChannelExportJob failedJob;
    failedJob.future = failedPromise.get_future();
    failedJob.channelLabel = "LFE";

    std::vector<AppState::ChannelExportJob> completionJobs;
    completionJobs.push_back(std::move(readyJob));
    completionJobs.emplace_back();
    completionJobs.push_back(std::move(failedJob));
    const auto completions =
        aura::audio_export::PollChannelExportCompletions(completionJobs);
    check(completions.size() == 2,
          "completion workflow consumes ready success and failure jobs");
    check(completionJobs.empty(),
          "completion workflow removes completed and invalid jobs");
    check(completions[0].channelLabel == "C" && completions[0].result.ok,
          "completion workflow preserves successful channel result");
    check(completions[1].channelLabel == "LFE" &&
              completions[1].result.errorMessage == "export future failed",
          "completion workflow normalizes future exceptions");

    fs::remove_all(tempDir, ec);
    if (failures) {
        std::cerr << failures << " failure(s)\n";
        return 1;
    }
    std::cout << "All single-channel export checks passed.\n";
    return 0;
}

// Regression coverage for processed drag/send exports. When multiple files are
// sent to a DAW with playback DSP active, each file must be rendered through
// AudioEngine::exportFileProcessed instead of falling back to raw source paths.
#include "src/AudioEngine.h"
#include "src/CenterSenseDataSource.h"
#include "src/DiagnosticsLogger.h"
#include "src/Localization.h"

#include "../thirdparty/miniaudio.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

static bool writeStereoToneWav(const fs::path& path, double leftHz, double rightHz,
                               double amplitude = 18000.0) {
    constexpr uint32_t sampleRate = 48000;
    constexpr uint32_t frameCount = 24000;
    constexpr uint16_t channels = 2;
    constexpr uint16_t bits = 16;
    constexpr double twoPi = 6.28318530717958647692;

    std::vector<int16_t> interleaved((size_t)frameCount * channels);
    for (uint32_t frame = 0; frame < frameCount; ++frame) {
        const double t = (double)frame / (double)sampleRate;
        interleaved[(size_t)frame * channels + 0] =
            (int16_t)(std::sin(twoPi * leftHz * t) * amplitude);
        interleaved[(size_t)frame * channels + 1] =
            (int16_t)(std::sin(twoPi * rightHz * t) * amplitude);
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

static bool writeSixChannelToneWav(const fs::path& path) {
    constexpr uint32_t sampleRate = 48000;
    constexpr uint32_t frameCount = 24000;
    constexpr uint16_t channels = 6;
    constexpr uint16_t bits = 16;
    constexpr double twoPi = 6.28318530717958647692;
    std::vector<int16_t> interleaved((size_t)frameCount * channels);
    for (uint32_t frame = 0; frame < frameCount; ++frame) {
        const double t = (double)frame / (double)sampleRate;
        for (uint16_t channel = 0; channel < channels; ++channel) {
            interleaved[(size_t)frame * channels + channel] =
                (int16_t)(std::sin(twoPi * (440.0 + 110.0 * channel) * t) * 12000.0);
        }
    }
    const uint32_t dataBytes = (uint32_t)(interleaved.size() * sizeof(int16_t));
    const uint32_t riffSize = 4 + (8 + 16) + (8 + dataBytes);
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    auto put32 = [&](uint32_t value) {
        const char bytes[4] = {char(value & 0xff), char((value >> 8) & 0xff),
                               char((value >> 16) & 0xff), char((value >> 24) & 0xff)};
        out.write(bytes, 4);
    };
    auto put16 = [&](uint16_t value) {
        const char bytes[2] = {char(value & 0xff), char((value >> 8) & 0xff)};
        out.write(bytes, 2);
    };
    out.write("RIFF", 4);
    put32(riffSize);
    out.write("WAVEfmt ", 8);
    put32(16);
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

static bool decodeFile(const fs::path& path,
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

static double channelRms(const std::vector<float>& samples,
                         uint32_t channels,
                         uint32_t channel,
                         size_t trimFrames) {
    if (channels == 0 || channel >= channels) return 0.0;
    const size_t frameCount = samples.size() / channels;
    if (frameCount <= trimFrames * 2) return 0.0;
    double sum = 0.0;
    size_t count = 0;
    for (size_t frame = trimFrames; frame < frameCount - trimFrames; ++frame) {
        const float s = samples[frame * channels + channel];
        sum += (double)s * (double)s;
        ++count;
    }
    return count > 0 ? std::sqrt(sum / (double)count) : 0.0;
}

static double estimateFirstChannelFrequencyHz(const std::vector<float>& samples,
                                              uint32_t channels,
                                              uint32_t sampleRate,
                                              size_t trimFrames) {
    if (channels == 0 || samples.empty()) return 0.0;
    const size_t frameCount = samples.size() / channels;
    if (frameCount <= trimFrames * 2) return 0.0;
    int positiveCrossings = 0;
    float prev = samples[trimFrames * channels];
    for (size_t frame = trimFrames + 1; frame < frameCount - trimFrames; ++frame) {
        const float curr = samples[frame * channels];
        if (prev <= 0.0f && curr > 0.0f) {
            ++positiveCrossings;
        }
        prev = curr;
    }
    const double durationSec =
        (double)(frameCount - trimFrames * 2) / (double)sampleRate;
    return durationSec > 0.0 ? (double)positiveCrossings / durationSec : 0.0;
}

static double channelToneRms(const std::vector<float>& samples, uint32_t channels,
                             uint32_t sampleRate, double frequencyHz, size_t trimFrames) {
    if (channels == 0 || sampleRate == 0) return 0.0;
    const size_t frames = samples.size() / channels;
    if (frames <= trimFrames * 2) return 0.0;
    double sineSum = 0.0, cosineSum = 0.0;
    constexpr double twoPi = 6.28318530717958647692;
    const size_t count = frames - trimFrames * 2;
    for (size_t frame = trimFrames; frame < frames - trimFrames; ++frame) {
        const double angle = twoPi * frequencyHz * static_cast<double>(frame) / sampleRate;
        const double value = samples[frame * channels];
        sineSum += value * std::sin(angle);
        cosineSum += value * std::cos(angle);
    }
    return std::sqrt(2.0) * std::hypot(sineSum, cosineSum) / static_cast<double>(count);
}

static std::vector<char> readFileBytes(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::vector<char>((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());
}

static std::string readEnvironmentVariable(const char* name) {
#if defined(_WIN32) && defined(_MSC_VER)
    char* value = nullptr;
    size_t length = 0;
    if (_dupenv_s(&value, &length, name) != 0 || !value) return {};
    std::string result(value);
    std::free(value);
    return result;
#else
    const char* value = std::getenv(name);
    return value ? std::string(value) : std::string();
#endif
}

static double samplesRmsDifference(const std::vector<float>& a,
                                   const std::vector<float>& b) {
    if (a.size() != b.size() || a.empty()) return 1.0e9;
    double sum = 0.0;
    for (size_t i = 0; i < a.size(); ++i) {
        const double delta = (double)a[i] - (double)b[i];
        sum += delta * delta;
    }
    return std::sqrt(sum / (double)a.size());
}

static void waitDiagDrain() {
    std::this_thread::sleep_for(std::chrono::milliseconds(650));
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

    const fs::path qaRoot = "D:/Aris-QA/RQ-20261014-01-magic-eq-rework-20261019";
    std::error_code ec;
    const auto runId = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path systemTemp = fs::temp_directory_path(ec);
    check(!ec && systemTemp.is_absolute(), "system TEMP directory is available");
    if (ec || !systemTemp.is_absolute()) return 1;
    const fs::path tempBase = systemTemp / "aris-processed-file-export-tests";
    fs::path tempDir = tempBase / std::to_string(runId);
    fs::create_directories(tempDir, ec);
    check(!ec, "isolated synthetic fixture directory created under TEMP");

    fs::path inA = tempDir / "impact_a.wav";
    fs::path inB = tempDir / "impact_b.wav";
    fs::path outVolA = tempDir / "impact_a_vol.wav";
    fs::path outVolB = tempDir / "impact_b_vol.wav";
    fs::path outMono = tempDir / "impact_a_mono.wav";
    fs::path outPitch = tempDir / "impact_a_pitch.wav";
    fs::path inMagicEq = tempDir / "magic_eq_tones.wav";
    fs::path inMagicPeak = tempDir / "magic_eq_peak_stress.wav";
    fs::path outMagicPeak = tempDir / "magic_eq_peak_protected.wav";
    fs::path outMagicPass = tempDir / "magic_eq_pass.wav";
    fs::path outMagicNotch = tempDir / "magic_eq_notch.wav";
    fs::path outMagicBypass = tempDir / "magic_eq_bypass.wav";
    fs::path outMagicCombined = tempDir / "magic_eq_with_volume.wav";
    fs::path outMagicFrozen = tempDir / "magic_eq_frozen_snapshot.wav";
    fs::path inSixChannel = tempDir / "six_channel_pcm.wav";
    fs::path rejectedSixChannel = tempDir / "six_channel_eq_rejected.wav";
    fs::path bypassSixChannel = tempDir / "six_channel_eq_disabled.wav";
    fs::path outRegionPass = tempDir / "magic_eq_selected_region_pass.wav";
    fs::path failedOutput = tempDir / "missing-parent" / "must-not-be-delivered.wav";

    auto& diag = DiagnosticsLogger::instance();
    diag.setEnabled(true);
    diag.setMaxEntries(512);
    diag.clear();

    check(writeStereoToneWav(inA, 440.0, 660.0), "first source WAV created");
    check(writeStereoToneWav(inB, 550.0, 770.0), "second source WAV created");
    check(writeStereoToneWav(inMagicEq, 1000.0, 5000.0),
          "Magic EQ pass/notch source WAV created with one in-band and one out-of-band tone");
    check(writeStereoToneWav(inMagicPeak, 1000.0, 1000.0, 32700.0),
          "isolated near-fullscale stereo stress WAV created");
    check(writeSixChannelToneWav(inSixChannel),
          "isolated six-channel PCM WAV created under TEMP");
    const std::vector<char> peakSourceBytes = readFileBytes(inMagicPeak);
    const std::vector<char> magicSourceBytes = readFileBytes(inMagicEq);

    std::vector<float> sourceA;
    uint32_t sourceChannels = 0;
    uint32_t sampleRate = 0;
    check(decodeFile(inA, sourceA, sourceChannels, sampleRate), "source decodes");
    check(sourceChannels == 2 && sampleRate == 48000, "source format is stereo 48 kHz");
    const double sourceRms = channelRms(sourceA, sourceChannels, 0, 800);

    MagicEQSettings passSettings;
    passSettings.enabled = true;
    passSettings.mode = CenterSenseDsp::MagicEQMode::Pass;
    passSettings.centerFreq = 1000.0f;
    passSettings.halfWidthPercent = 8.0f;
    AudioEngine passEngine;
    passEngine.setMagicEQ(passSettings);
    {
        MagicEQSettings invalid = passSettings;
        invalid.centerFreq = std::numeric_limits<float>::quiet_NaN();
        passEngine.setMagicEQ(invalid);
        const auto rejected = passEngine.getMagicEQ();
        check(!rejected.enabled && rejected.invalidInput,
              "invalid Magic EQ input becomes a visible bypass instead of prior enabled sound");
        passEngine.setMagicEQ(passSettings);
    }
    const auto overloadSnapshot = passEngine.captureTransferProcessingSnapshot();
    check(!passEngine.exportFileProcessed(inSixChannel.string(), rejectedSixChannel.string(), &overloadSnapshot),
          "enabled Magic EQ rejects six-channel delivery");
    check(!fs::exists(rejectedSixChannel),
          "rejected six-channel delivery does not create a processed file");
    check(!passEngine.getLastExportError().empty() &&
              passEngine.getLastExportError() == L(MagicEQ_UnsupportedChannels),
          "rejected six-channel delivery exposes the localized channel error");
    MagicEQSettings sixChannelBypass = passSettings;
    sixChannelBypass.enabled = false;
    AudioEngine sixChannelBypassEngine;
    sixChannelBypassEngine.setMagicEQ(sixChannelBypass);
    const auto sixChannelBypassSnapshot = sixChannelBypassEngine.captureTransferProcessingSnapshot();
    check(sixChannelBypassEngine.exportFileProcessed(
              inSixChannel.string(), bypassSixChannel.string(), &sixChannelBypassSnapshot),
          "disabled Magic EQ still exports six-channel PCM");
    std::vector<float> sixChannelBypassSamples;
    uint32_t sixChannelBypassChannels = 0, sixChannelBypassRate = 0;
    check(decodeFile(bypassSixChannel, sixChannelBypassSamples,
                     sixChannelBypassChannels, sixChannelBypassRate) &&
              sixChannelBypassChannels == 6 && sixChannelBypassRate == 48000 &&
              !sixChannelBypassSamples.empty(),
          "disabled Magic EQ preserves the six-channel output layout");
    check(passEngine.exportFileProcessed(inMagicPeak.string(), outMagicPeak.string(), &overloadSnapshot),
          "overload Magic EQ DAW delivery renders a real WAV");
    std::vector<float> overloadDelivered;
    uint32_t overloadChannels = 0, overloadRate = 0;
    check(decodeFile(outMagicPeak, overloadDelivered, overloadChannels, overloadRate),
          "overload delivery WAV decodes");
    float overloadDeliveredPeak = 0.0f;
    bool overloadFinite = true;
    for (float sample : overloadDelivered) {
        overloadFinite = overloadFinite && std::isfinite(sample);
        overloadDeliveredPeak = std::max(overloadDeliveredPeak, std::abs(sample));
    }
    std::cout << "MagicEQDeliveredPCM sample_peak=" << overloadDeliveredPeak
              << " channels=" << overloadChannels << " sample_rate=" << overloadRate << '\n';
    check(overloadFinite && overloadDeliveredPeak <= 1.0f,
          "actual DAW delivery WAV has finite in-range PCM samples");
    check(readFileBytes(inMagicPeak) == peakSourceBytes,
          "overload delivery leaves original source WAV byte-for-byte unchanged");
    {
        Platform::AudioDataSource::Decoder decoder;
        const auto opened = Platform::AudioDataSource::InitializeDecoderFromFileWithFormat(
            decoder, inMagicPeak.string(), 2, 48000);
        check(opened.Succeeded(), "formal playback decoder opens isolated overload WAV");
        CenterSenseDsp::MagicEQSettingsPublisher publisher;
        publisher.store(passSettings);
        float centerSense = 0.0f;
        CenterSenseDataSource playback;
        const bool initialized = opened.Succeeded() && playback.init(
            Platform::AudioDataSource::BorrowDataSource(decoder), 48000,
            &centerSense, nullptr, nullptr, nullptr, &publisher);
        check(initialized, "formal playback transform source initializes with Magic EQ");
        if (initialized) {
            auto transformed = Platform::AudioDataSource::BorrowDataSource(playback.getDataSource());
            std::vector<float> played(4096 * 2);
            float playedPeak = 0.0f;
            bool playedFinite = true;
            size_t framesReadTotal = 0;
            std::vector<float> firstPlayed;
            for (;;) {
                const auto result = Platform::AudioDataSource::ReadPcmFrames(
                    transformed, played.data(), 4096);
                if (result.backendCode != 0 || result.framesRead == 0) break;
                framesReadTotal += static_cast<size_t>(result.framesRead);
                if (firstPlayed.empty())
                    firstPlayed.assign(played.begin(), played.begin() + static_cast<size_t>(result.framesRead) * 2);
                for (size_t index = 0; index < static_cast<size_t>(result.framesRead) * 2; ++index) {
                    playedFinite = playedFinite && std::isfinite(played[index]);
                    playedPeak = std::max(playedPeak, std::abs(played[index]));
                }
            }
            std::cout << "MagicEQPlaybackFloat sample_peak=" << playedPeak
                      << " frames=" << framesReadTotal << '\n';
            check(playedFinite && playedPeak <= 1.0f && framesReadTotal > 0,
                  "formal playback transform emits finite in-range float frames");
            const auto seek = Platform::AudioDataSource::SeekPcmFrame(transformed, 0);
            check(seek.Succeeded(), "formal playback transform seeks to source start");
            const auto afterSeek = Platform::AudioDataSource::ReadPcmFrames(
                transformed, played.data(), 4096);
            check(afterSeek.backendCode == 0 && afterSeek.framesRead > 0,
                  "formal playback transform resumes after seek with reset state");
            bool seekReproducible = afterSeek.framesRead * 2 == firstPlayed.size();
            if (seekReproducible) {
                for (size_t index = 0; index < firstPlayed.size(); ++index)
                    seekReproducible = seekReproducible && std::abs(played[index] - firstPlayed[index]) < 1e-6f;
            }
            check(seekReproducible, "seek clears Magic EQ filter and protection history");
            MagicEQSettings switchedOff = passSettings;
            switchedOff.enabled = false;
            publisher.store(switchedOff);
            const auto bypassSeek = Platform::AudioDataSource::SeekPcmFrame(transformed, 0);
            check(bypassSeek.Succeeded(), "bypass seek resets formal playback stream");
            const auto bypassRead = Platform::AudioDataSource::ReadPcmFrames(
                transformed, played.data(), 4096);
            std::vector<float> sourceDecoded;
            uint32_t decodedChannels = 0, decodedRate = 0;
            const bool sourceDecodedOkay = decodeFile(inMagicPeak, sourceDecoded, decodedChannels, decodedRate);
            bool bypassUnchanged = sourceDecodedOkay && bypassRead.backendCode == 0 &&
                bypassRead.framesRead * 2 <= sourceDecoded.size();
            if (bypassUnchanged) {
                for (size_t index = 0; index < static_cast<size_t>(bypassRead.framesRead) * 2; ++index)
                    bypassUnchanged = bypassUnchanged && std::abs(played[index] - sourceDecoded[index]) < 1e-6f;
            }
            check(bypassUnchanged, "formal playback bypass restores unmodified source samples");
            playback.uninit();
        }
        Platform::AudioDataSource::UninitializeDecoder(decoder);
    }
    {
        // Compare real decoded playback frames with the same source and EQ bypassed.
        Platform::AudioDataSource::Decoder decoder;
        const auto opened = Platform::AudioDataSource::InitializeDecoderFromFileWithFormat(
            decoder, inMagicEq.string(), 2, 48000);
        check(opened.Succeeded(), "normal playback decoder opens isolated Magic EQ WAV");
        CenterSenseDsp::MagicEQSettingsPublisher publisher;
        publisher.store(passSettings);
        float centerSense = 0.0f;
        CenterSenseDataSource playback;
        const bool initialized = opened.Succeeded() && playback.init(
            Platform::AudioDataSource::BorrowDataSource(decoder), 48000,
            &centerSense, nullptr, nullptr, nullptr, &publisher);
        check(initialized, "normal playback decoder connects to Magic EQ source");
        if (initialized) {
            auto source = Platform::AudioDataSource::BorrowDataSource(playback.getDataSource());
            std::vector<float> enabled(4096 * 2);
            std::vector<float> bypassed(4096 * 2);
            const auto onRead = Platform::AudioDataSource::ReadPcmFrames(source, enabled.data(), 4096);
            MagicEQSettings disabledSettings = passSettings;
            disabledSettings.enabled = false;
            publisher.store(disabledSettings);
            const auto seek = Platform::AudioDataSource::SeekPcmFrame(source, 0);
            const auto offRead = Platform::AudioDataSource::ReadPcmFrames(source, bypassed.data(), 4096);
            double differenceEnergy = 0.0;
            double enabledEnergy = 0.0;
            double bypassEnergy = 0.0;
            for (size_t index = 0; index < enabled.size(); ++index) {
                const double delta = enabled[index] - bypassed[index];
                differenceEnergy += delta * delta;
                enabledEnergy += static_cast<double>(enabled[index]) * enabled[index];
                bypassEnergy += static_cast<double>(bypassed[index]) * bypassed[index];
            }
            check(onRead.backendCode == 0 && onRead.framesRead == 4096 &&
                      seek.Succeeded() && offRead.backendCode == 0 && offRead.framesRead == 4096 &&
                      enabledEnergy > 0.0 && bypassEnergy > 0.0 &&
                      std::sqrt(differenceEnergy / enabled.size()) > 0.01,
                  "normal decoded playback has nonzero EQ output distinct from bypass");
            playback.uninit();
        }
        Platform::AudioDataSource::UninitializeDecoder(decoder);
    }
    const auto passSnapshot = passEngine.captureTransferProcessingSnapshot();
    check(passEngine.exportFileProcessed(inMagicEq.string(), outMagicPass.string(), &passSnapshot),
          "Magic EQ Pass export renders a WAV");

    std::vector<float> passSamples;
    uint32_t passChannels = 0;
    uint32_t passRate = 0;
    check(decodeFile(outMagicPass, passSamples, passChannels, passRate),
          "Magic EQ Pass output decodes");
    check(passChannels == 2 && passRate == 48000,
          "Magic EQ Pass output preserves stereo 48 kHz format");
    std::vector<float> magicSourceSamples;
    uint32_t magicSourceChannels = 0;
    uint32_t magicSourceRate = 0;
    check(decodeFile(inMagicEq, magicSourceSamples, magicSourceChannels, magicSourceRate),
          "Magic EQ source decodes");
    const double magicLeftRms = channelRms(magicSourceSamples, magicSourceChannels, 0, 800);
    const double magicRightRms = channelRms(magicSourceSamples, magicSourceChannels, 1, 800);
    const double passLeftRatio = channelRms(passSamples, passChannels, 0, 800) / magicLeftRms;
    const double passRightRatio = channelRms(passSamples, passChannels, 1, 800) / magicRightRms;
    std::cout << "MagicEqPass inBandRatio=" << passLeftRatio
              << " outOfBandRatio=" << passRightRatio << "\n";
    check(passLeftRatio > 0.65, "Pass keeps the selected 1 kHz tone");
    check(passRightRatio < 0.20, "Pass suppresses the out-of-selection 5 kHz tone");

    MagicEQSettings notchSettings = passSettings;
    notchSettings.mode = CenterSenseDsp::MagicEQMode::Notch;
    notchSettings.halfWidthPercent = 2.5f;
    AudioEngine notchEngine;
    notchEngine.setMagicEQ(notchSettings);
    const auto notchSnapshot = notchEngine.captureTransferProcessingSnapshot();
    check(notchEngine.exportFileProcessed(inMagicEq.string(), outMagicNotch.string(), &notchSnapshot),
          "Magic EQ Notch export renders a WAV");
    std::vector<float> notchSamples;
    uint32_t notchChannels = 0;
    uint32_t notchRate = 0;
    check(decodeFile(outMagicNotch, notchSamples, notchChannels, notchRate),
          "Magic EQ Notch output decodes");
    const double notchLeftRatio = channelRms(notchSamples, notchChannels, 0, 800) / magicLeftRms;
    const double notchRightRatio = channelRms(notchSamples, notchChannels, 1, 800) / magicRightRms;
    std::cout << "MagicEqNotch centerRatio=" << notchLeftRatio
              << " outsideRatio=" << notchRightRatio << "\n";
    check(notchLeftRatio < 0.20, "Notch suppresses the selected 1 kHz tone");
    check(notchRightRatio > 0.65, "Notch preserves the out-of-selection 5 kHz tone");

    // Measure the actual AudioEngine WAV output for every slope and both modes.
    const double probes[] = {200.0, 700.0, 850.0, 1000.0, 1180.0, 1450.0, 5000.0};
    const CenterSenseDsp::MagicEQSteepness slopes[] = {
        CenterSenseDsp::MagicEQSteepness::Soft,
        CenterSenseDsp::MagicEQSteepness::Steep,
        CenterSenseDsp::MagicEQSteepness::VerySteep,
        CenterSenseDsp::MagicEQSteepness::Brickwall,
    };
    for (const auto mode : {CenterSenseDsp::MagicEQMode::Pass, CenterSenseDsp::MagicEQMode::Notch}) {
        double ratios[4][7]{};
        for (int slope = 0; slope < 4; ++slope) {
            for (int point = 0; point < 7; ++point) {
                const fs::path source = tempDir / ("slope-source-" + std::to_string(point) + ".wav");
                const fs::path output = tempDir / ("slope-output-" + std::to_string(static_cast<int>(mode)) +
                    "-" + std::to_string(slope) + "-" + std::to_string(point) + ".wav");
                check(writeStereoToneWav(source, probes[point], probes[point]),
                      "slope source tone created");
                const auto sourceBytes = readFileBytes(source);
                MagicEQSettings selected = passSettings;
                selected.mode = mode;
                selected.steepness = slopes[slope];
                selected.halfWidthPercent = 5.0f;
                AudioEngine engine;
                engine.setMagicEQ(selected);
                const auto snapshot = engine.captureTransferProcessingSnapshot();
                selected.steepness = slopes[(slope + 1) % 4];
                engine.setMagicEQ(selected);
                check(engine.exportFileProcessed(source.string(), output.string(), &snapshot),
                      "slope output renders from frozen AudioEngine snapshot");
                std::vector<float> rendered;
                uint32_t channels = 0, rate = 0;
                check(decodeFile(output, rendered, channels, rate), "slope output WAV decodes");
                ratios[slope][point] = channelToneRms(rendered, channels, rate, probes[point], 4000) /
                    (18000.0 / 32768.0 / std::sqrt(2.0));
                check(channels == 2 && rate == 48000 && readFileBytes(source) == sourceBytes,
                      "slope output keeps format and source bytes");
            }
            CenterSenseDsp::MagicEQProcessParams displayedParams{};
            displayedParams.enabled = true;
            displayedParams.mode = mode;
            displayedParams.steepness = slopes[slope];
            displayedParams.centerFreq = 1000.0f;
            displayedParams.halfWidthPercent = 5.0f;
            const auto displayed = CenterSenseDsp::measureMagicEQLocalSlope(displayedParams, 48000);
            for (int sideIndex = 0; sideIndex < 2; ++sideIndex) {
                const auto& side = sideIndex == 0 ? displayed.low : displayed.high;
                check(side.valid, "frozen export has a valid current-band slope interval");
                if (!side.valid) continue;
                double renderedGain[2]{};
                for (int endpoint = 0; endpoint < 2; ++endpoint) {
                    const double frequency = endpoint == 0 ? side.fromHz : side.toHz;
                    const fs::path source = tempDir / ("live-slope-source-" +
                        std::to_string(sideIndex) + "-" + std::to_string(endpoint) + ".wav");
                    const fs::path output = tempDir / ("live-slope-output-" +
                        std::to_string(static_cast<int>(mode)) + "-" + std::to_string(slope) + "-" +
                        std::to_string(sideIndex) + "-" + std::to_string(endpoint) + ".wav");
                    check(writeStereoToneWav(source, frequency, frequency),
                          "current-band slope source tone created");
                    const auto sourceBytes = readFileBytes(source);
                    MagicEQSettings selected = passSettings;
                    selected.mode = mode;
                    selected.steepness = slopes[slope];
                    selected.halfWidthPercent = 5.0f;
                    AudioEngine engine;
                    engine.setMagicEQ(selected);
                    const auto snapshot = engine.captureTransferProcessingSnapshot();
                    selected.steepness = slopes[(slope + 1) % 4];
                    selected.centerFreq = 2000.0f;
                    engine.setMagicEQ(selected);
                    check(engine.exportFileProcessed(source.string(), output.string(), &snapshot),
                          "current-band slope WAV uses frozen mode, band and grade");
                    std::vector<float> rendered;
                    uint32_t channels = 0, rate = 0;
                    check(decodeFile(output, rendered, channels, rate),
                          "frozen current-band slope WAV decodes");
                    renderedGain[endpoint] = channelToneRms(rendered, channels, rate, frequency, 4000);
                    check(readFileBytes(source) == sourceBytes,
                          "current-band slope source remains byte-identical");
                }
                const double renderedSlope = 20.0 * std::log10(renderedGain[1] / renderedGain[0]) /
                    std::log2(side.toHz / side.fromHz);
                std::cout << "FrozenLiveSlope mode=" << static_cast<int>(mode)
                          << " grade=" << slope << " side=" << sideIndex
                          << " display=" << side.dbPerOctave << " rendered=" << renderedSlope << "\n";
                check(std::isfinite(renderedSlope) &&
                      std::fabs(renderedSlope - side.dbPerOctave) < 1.0,
                      "frozen AudioEngine WAV matches current-band local slope display");
            }
            std::cout << "RenderedSlope mode=" << static_cast<int>(mode) << " slope=" << slope;
            for (double ratio : ratios[slope]) std::cout << " ratio=" << ratio;
            std::cout << "\n";
        }
        const int nearLow = 2, center = 3, nearHigh = 4;
        if (mode == CenterSenseDsp::MagicEQMode::Notch) {
            check(ratios[0][nearLow] > ratios[1][nearLow] && ratios[1][nearLow] > ratios[2][nearLow] &&
                      ratios[2][nearLow] > ratios[3][nearLow],
                  "rendered Notch low edge steepens across four frozen choices");
            check(ratios[0][nearHigh] > ratios[1][nearHigh] && ratios[1][nearHigh] > ratios[2][nearHigh] &&
                      ratios[2][nearHigh] > ratios[3][nearHigh],
                  "rendered Notch high edge steepens across four frozen choices");
            for (int slope = 0; slope < 4; ++slope)
                check(ratios[slope][center] < 0.05 && ratios[slope][0] > 0.85 && ratios[slope][6] > 0.85,
                      "rendered Notch removes center and preserves remote tones");
        } else {
            // Pass's 850/1180 Hz probes are inside the kept band; use 700/1450 Hz
            // outside the two edges when checking rejection growth.
            const int outsideLow = 1, outsideHigh = 5;
            check(ratios[0][outsideLow] > ratios[1][outsideLow] &&
                      ratios[1][outsideLow] > ratios[2][outsideLow] &&
                      ratios[2][outsideLow] > ratios[3][outsideLow],
                  "rendered Pass low edge steepens across four frozen choices");
            check(ratios[0][outsideHigh] > ratios[1][outsideHigh] &&
                      ratios[1][outsideHigh] > ratios[2][outsideHigh] &&
                      ratios[2][outsideHigh] > ratios[3][outsideHigh],
                  "rendered Pass high edge steepens across four frozen choices");
            for (int slope = 0; slope < 4; ++slope)
                check(ratios[slope][center] > 0.85 && ratios[slope][0] < 0.1 && ratios[slope][6] < 0.1,
                      "rendered Pass keeps center and suppresses remote tones");
        }
    }

    // An unavailable frozen combination must fail before creating a delivery file.
    const fs::path rejectedOutput = tempDir / "unavailable-brickwall.wav";
    MagicEQSettings unavailable = passSettings;
    unavailable.steepness = CenterSenseDsp::MagicEQSteepness::Brickwall;
    unavailable.halfWidthPercent = 1.0f;
    AudioEngine rejectedEngine;
    rejectedEngine.setMagicEQ(unavailable);
    const auto rejectedSnapshot = rejectedEngine.captureTransferProcessingSnapshot();
    check(!rejectedEngine.exportFileProcessed(inMagicEq.string(), rejectedOutput.string(), &rejectedSnapshot) &&
              !fs::exists(rejectedOutput),
          "unavailable frozen near-brickwall export reports failure without a delivery file");

    MagicEQSettings bypassSettings = passSettings;
    bypassSettings.enabled = false;
    AudioEngine bypassEngine;
    bypassEngine.setMagicEQ(bypassSettings);
    const auto bypassSnapshot = bypassEngine.captureTransferProcessingSnapshot();
    check(bypassEngine.exportFileProcessed(inMagicEq.string(), outMagicBypass.string(), &bypassSnapshot),
          "Magic EQ bypass export renders a WAV");
    std::vector<float> bypassSamples;
    uint32_t bypassChannels = 0;
    uint32_t bypassRate = 0;
    check(decodeFile(outMagicBypass, bypassSamples, bypassChannels, bypassRate),
          "Magic EQ bypass output decodes");
    const double bypassLeftRatio = channelRms(bypassSamples, bypassChannels, 0, 800) / magicLeftRms;
    const double bypassRightRatio = channelRms(bypassSamples, bypassChannels, 1, 800) / magicRightRms;
    check(std::abs(bypassLeftRatio - 1.0) < 0.03 && std::abs(bypassRightRatio - 1.0) < 0.03,
          "Magic EQ bypass leaves both source tones unchanged");

    const std::string realFixtureEnv = readEnvironmentVariable("ARIS_MAGIC_EQ_REAL_FIXTURE");
    if (!realFixtureEnv.empty()) {
        const fs::path realFixture(realFixtureEnv);
        const fs::path fixtureRoot = fs::weakly_canonical(qaRoot / "fixture", ec);
        const fs::path canonicalFixture = fs::weakly_canonical(realFixture, ec);
        const fs::path fixtureRelative = canonicalFixture.lexically_relative(fixtureRoot);
        const bool fixtureUnderQaRoot = !ec && !fixtureRelative.empty() &&
                                        fixtureRelative.is_relative() &&
                                        *fixtureRelative.begin() != "..";
        const fs::path realPassPath = tempDir / "real_magic_eq_pass.wav";
        const fs::path realNotchPath = tempDir / "real_magic_eq_notch.wav";
        const fs::path realBypassPath = tempDir / "real_magic_eq_bypass.wav";
        const bool fixtureExists = fs::exists(realFixture);
        check(fixtureExists && fixtureUnderQaRoot,
              "provided real-audio fixture exists under the task QA root");
        if (fixtureExists && fixtureUnderQaRoot) {
            const std::vector<char> realFixtureBytes = readFileBytes(realFixture);
            std::vector<float> realSourceSamples;
            uint32_t realSourceChannels = 0, realSourceRate = 0;
            check(decodeFile(realFixture, realSourceSamples, realSourceChannels, realSourceRate),
                  "real-audio source fixture decodes");
            AudioEngine realPassEngine;
            realPassEngine.setMagicEQ(passSettings);
            const auto realPassSnapshot = realPassEngine.captureTransferProcessingSnapshot();
            check(realPassEngine.exportFileProcessed(realFixture.string(), realPassPath.string(),
                                                     &realPassSnapshot),
                  "real-audio Magic EQ Pass creates a rendered WAV");
            AudioEngine realNotchEngine;
            realNotchEngine.setMagicEQ(notchSettings);
            const auto realNotchSnapshot = realNotchEngine.captureTransferProcessingSnapshot();
            check(realNotchEngine.exportFileProcessed(realFixture.string(), realNotchPath.string(),
                                                      &realNotchSnapshot),
                  "real-audio Magic EQ Notch creates a rendered WAV");
            AudioEngine realBypassEngine;
            realBypassEngine.setMagicEQ(bypassSettings);
            const auto realBypassSnapshot = realBypassEngine.captureTransferProcessingSnapshot();
            check(realBypassEngine.exportFileProcessed(realFixture.string(), realBypassPath.string(),
                                                       &realBypassSnapshot),
                  "real-audio Magic EQ bypass creates a rendered WAV");

            std::vector<float> realPassSamples, realNotchSamples, realBypassSamples;
            uint32_t realPassChannels = 0, realNotchChannels = 0, realBypassChannels = 0;
            uint32_t realPassRate = 0, realNotchRate = 0, realBypassRate = 0;
            check(decodeFile(realPassPath, realPassSamples, realPassChannels, realPassRate),
                  "real-audio Pass WAV decodes");
            check(decodeFile(realNotchPath, realNotchSamples, realNotchChannels, realNotchRate),
                  "real-audio Notch WAV decodes");
            check(decodeFile(realBypassPath, realBypassSamples, realBypassChannels, realBypassRate),
                  "real-audio bypass WAV decodes");
            const double realPassDelta = samplesRmsDifference(realPassSamples, realBypassSamples);
            const double realNotchDelta = samplesRmsDifference(realNotchSamples, realBypassSamples);
            const double realBypassDelta = samplesRmsDifference(realSourceSamples, realBypassSamples);
            std::cout << "RealFixtureMagicEQ passVsBypassRms=" << realPassDelta
                      << " notchVsBypassRms=" << realNotchDelta
                      << " bypassVsDecodedSourceRms=" << realBypassDelta << "\n";
            check(realPassChannels == realBypassChannels && realPassRate == realBypassRate &&
                      realPassSamples.size() == realBypassSamples.size(),
                  "real-audio Pass retains the bypass output format and duration");
            check(realNotchChannels == realBypassChannels && realNotchRate == realBypassRate &&
                      realNotchSamples.size() == realBypassSamples.size(),
                  "real-audio Notch retains the bypass output format and duration");
            check(realPassDelta > 0.001,
                  "real-audio Pass output contains a measurable Magic EQ change");
            check(realNotchDelta > 0.001,
                  "real-audio Notch output contains a measurable Magic EQ change");
            check(realPassChannels == 2 && realPassRate == 44100,
                  "real-audio rendering preserves source stereo 44.1 kHz format");
            check(realBypassDelta < 0.02,
                  "real-audio bypass matches decoded source apart from the established edge fade");
            check(readFileBytes(realFixture) == realFixtureBytes,
                  "real-audio fixture remains byte-identical after all renders");
        }
    } else {
        std::cout << "SKIP: ARIS_MAGIC_EQ_REAL_FIXTURE is unset; synthetic WAV assertions still run.\n";
    }

    AudioEngine combinedEngine;
    combinedEngine.setVolume(0.5f);
    combinedEngine.setMagicEQ(passSettings);
    const auto combinedSnapshot = combinedEngine.captureTransferProcessingSnapshot();
    check(combinedEngine.exportFileProcessed(inMagicEq.string(), outMagicCombined.string(),
                                             &combinedSnapshot),
          "Magic EQ and existing volume processing render together");
    std::vector<float> combinedSamples;
    uint32_t combinedChannels = 0;
    uint32_t combinedRate = 0;
    check(decodeFile(outMagicCombined, combinedSamples, combinedChannels, combinedRate),
          "combined Magic EQ output decodes");
    const double combinedLeftRatio = channelRms(combinedSamples, combinedChannels, 0, 800) / magicLeftRms;
    const double combinedRightRatio = channelRms(combinedSamples, combinedChannels, 1, 800) / magicRightRms;
    check(combinedLeftRatio > 0.30 && combinedLeftRatio < 0.65,
          "existing volume gain remains applied with Magic EQ Pass");
    check(combinedRightRatio < 0.10,
          "Magic EQ band processing remains active with existing volume gain");

    AudioEngine frozenEngine;
    frozenEngine.setVolume(0.5f);
    frozenEngine.setMagicEQ(passSettings);
    const auto frozenSnapshot = frozenEngine.captureTransferProcessingSnapshot();
    frozenEngine.setVolume(0.25f);
    frozenEngine.setMagicEQ(notchSettings);
    check(frozenEngine.exportFileProcessed(inMagicEq.string(), outMagicFrozen.string(),
                                            &frozenSnapshot),
          "frozen transfer snapshot renders despite later UI-setting changes");
    std::vector<float> frozenSamples;
    uint32_t frozenChannels = 0;
    uint32_t frozenRate = 0;
    check(decodeFile(outMagicFrozen, frozenSamples, frozenChannels, frozenRate),
          "frozen transfer output decodes");
    const double frozenLeftRatio = channelRms(frozenSamples, frozenChannels, 0, 800) / magicLeftRms;
    const double frozenRightRatio = channelRms(frozenSamples, frozenChannels, 1, 800) / magicRightRms;
    check(frozenLeftRatio > 0.30 && frozenLeftRatio < 0.65 && frozenRightRatio < 0.10,
          "render uses captured Pass plus captured gain instead of later Notch/gain settings");

    AudioEngine failedRenderEngine;
    failedRenderEngine.setMagicEQ(passSettings);
    const auto failedRenderSnapshot = failedRenderEngine.captureTransferProcessingSnapshot();
    check(!failedRenderEngine.exportFileProcessed(inMagicEq.string(), failedOutput.string(),
                                                   &failedRenderSnapshot),
          "processed render failure is reported");
    check(!fs::exists(failedOutput),
          "failed render creates no path that could be handed to the DAW");
    check(readFileBytes(inMagicEq) == magicSourceBytes,
          "Magic EQ renders never modify the source WAV");

    AudioEngine regionEngine;
    const bool regionAudioReady = regionEngine.initialize(10);
    check(regionAudioReady, "AudioEngine initializes for selected-region transfer rendering");
    if (regionAudioReady) {
        regionEngine.setVolume(0.0f);
        const bool regionLoaded = regionEngine.loadFile(inMagicEq.string());
        check(regionLoaded, "isolated WAV loads for selected-region rendering");
        if (regionLoaded) {
            regionEngine.setMagicEQ(unavailable);
            check(!regionEngine.getMagicEQ().enabled,
                  "loaded playback visibly bypasses an unavailable frozen near-brickwall band");
            regionEngine.setVolume(1.0f);
            regionEngine.setInPoint(0.25f);
            regionEngine.setOutPoint(0.75f);
            regionEngine.setMagicEQ(passSettings);
            const auto regionSnapshot = regionEngine.captureTransferProcessingSnapshot();
            ExportRegionInfo regionInfo{};
            check(regionEngine.exportRegion(outRegionPass.string(), 0.0f, 0.0f, &regionInfo,
                                            &regionSnapshot),
                  "selected-region transfer renderer applies the frozen Magic EQ snapshot");
            std::vector<float> regionSamples;
            uint32_t regionChannels = 0, regionRate = 0;
            check(decodeFile(outRegionPass, regionSamples, regionChannels, regionRate),
                  "selected-region Pass WAV decodes");
            check(regionInfo.framesWritten == 12000 &&
                      regionSamples.size() / (regionChannels ? regionChannels : 1) == 12000,
                  "selected-region output contains only the requested half-file range");
            const double regionLeftRatio = channelRms(regionSamples, regionChannels, 0, 500) / magicLeftRms;
            const double regionRightRatio = channelRms(regionSamples, regionChannels, 1, 500) / magicRightRms;
            check(regionRate == 48000 && regionChannels == 2,
                  "selected-region output preserves stereo 48 kHz format");
            check(regionLeftRatio > 0.60 && regionRightRatio < 0.20,
                  "selected-region output applies Pass to the selected frequency range");
            regionEngine.setMagicEQ(notchSettings);
            const auto notchRegionSnapshot = regionEngine.captureTransferProcessingSnapshot();
            const fs::path notchRegionPath = tempDir / "magic_eq_selected_region_notch.wav";
            check(regionEngine.exportRegion(notchRegionPath.string(), 0.0f, 0.0f,
                                            nullptr, &notchRegionSnapshot),
                  "selected-region renderer exports a Notch WAV");
            std::vector<float> notchRegionSamples;
            uint32_t notchRegionChannels = 0, notchRegionRate = 0;
            check(decodeFile(notchRegionPath, notchRegionSamples, notchRegionChannels,
                             notchRegionRate), "selected-region Notch WAV decodes");
            check(notchRegionRate == 48000 && notchRegionChannels == 2 &&
                      channelRms(notchRegionSamples, notchRegionChannels, 0, 500) /
                          magicLeftRms < 0.20 &&
                      channelRms(notchRegionSamples, notchRegionChannels, 1, 500) /
                          magicRightRms > 0.60,
                  "selected-region output applies Notch to the selected frequency range");
            regionEngine.setMagicEQ(bypassSettings);
            const auto bypassRegionSnapshot = regionEngine.captureTransferProcessingSnapshot();
            const fs::path bypassRegionPath = tempDir / "magic_eq_selected_region_bypass.wav";
            ExportRegionInfo bypassRegionInfo{};
            check(regionEngine.exportRegion(bypassRegionPath.string(), 0.0f, 0.0f,
                                            &bypassRegionInfo, &bypassRegionSnapshot),
                  "selected-region renderer exports a bypassed WAV");
            std::vector<float> bypassRegionSamples;
            uint32_t bypassRegionChannels = 0, bypassRegionRate = 0;
            check(decodeFile(bypassRegionPath, bypassRegionSamples, bypassRegionChannels,
                             bypassRegionRate), "selected-region bypass WAV decodes");
            check(bypassRegionInfo.framesWritten == regionInfo.framesWritten &&
                      bypassRegionRate == 48000 && bypassRegionChannels == 2 &&
                      std::abs(channelRms(bypassRegionSamples, bypassRegionChannels, 0, 500) /
                               magicLeftRms - 1.0) < 0.03 &&
                      std::abs(channelRms(bypassRegionSamples, bypassRegionChannels, 1, 500) /
                               magicRightRms - 1.0) < 0.03,
                  "selected-region bypass preserves the source tones and range");
            check(readFileBytes(inMagicEq) == magicSourceBytes,
                  "selected-region rendering leaves its source WAV unchanged");
        }
        regionEngine.shutdown();
    }

    AudioEngine volumeEngine;
    volumeEngine.setVolume(0.5f);
    check(volumeEngine.hasAudioAdjustments(), "volume setting marks export as processed");
    check(volumeEngine.exportFileProcessed(inA.string(), outVolA.string()), "first processed export succeeds");
    check(volumeEngine.exportFileProcessed(inB.string(), outVolB.string()), "second processed export succeeds");
    waitDiagDrain();
    std::string exportDiag = diag.getRecentJsonLines(24);
    check(exportDiag.find("ExportFileProcessedStart: in=") != std::string::npos,
          "processed export emits start diagnostics");
    check(exportDiag.find("ExportFileProcessed: in=") != std::string::npos,
          "processed export emits completion diagnostics");

    std::vector<float> volA;
    uint32_t volChannels = 0;
    uint32_t volRate = 0;
    check(decodeFile(outVolA, volA, volChannels, volRate), "processed volume export decodes");
    check(volChannels == 2 && volRate == 48000, "volume export preserves stereo format");
    const double volumeRatio = channelRms(volA, volChannels, 0, 800) / sourceRms;
    check(volumeRatio > 0.45 && volumeRatio < 0.55, "volume export applies current gain");

    std::vector<float> volB;
    uint32_t volBChannels = 0;
    uint32_t volBRate = 0;
    check(decodeFile(outVolB, volB, volBChannels, volBRate), "second processed export decodes");
    check(volBChannels == 2 && volBRate == 48000, "second processed export is a rendered WAV");

    AudioEngine monoEngine;
    monoEngine.setMono(true);
    check(monoEngine.hasAudioAdjustments(), "mono setting marks export as processed");
    check(monoEngine.exportFileProcessed(inA.string(), outMono.string()), "mono processed export succeeds");
    std::vector<float> mono;
    uint32_t monoChannels = 0;
    uint32_t monoRate = 0;
    check(decodeFile(outMono, mono, monoChannels, monoRate), "mono export decodes");
    check(monoChannels == 1 && monoRate == 48000, "mono export writes a mono WAV");

    AudioEngine pitchEngine;
    pitchEngine.setPitch(12.0f);
    check(pitchEngine.hasAudioAdjustments(), "pitch setting marks export as processed");
    check(pitchEngine.exportFileProcessed(inA.string(), outPitch.string()), "pitch processed export succeeds");
    std::vector<float> pitch;
    uint32_t pitchChannels = 0;
    uint32_t pitchRate = 0;
    check(decodeFile(outPitch, pitch, pitchChannels, pitchRate), "pitch export decodes");
    const double pitchedHz = estimateFirstChannelFrequencyHz(pitch, pitchChannels, pitchRate, 800);
    check(pitchedHz > 850.0 && pitchedHz < 910.0, "pitch export applies +12 semitones");

    diag.clear();
    AudioEngine failEngine;
    check(!failEngine.exportFileProcessed("", outPitch.string()),
          "empty input processed export fails");
    waitDiagDrain();
    std::string exportFailDiag = diag.getRecentJsonLines(12);
    check(exportFailDiag.find("ExportFileProcessedFailed: reason=empty_input") != std::string::npos,
          "processed export failure emits diagnostics");

    if (realFixtureEnv.empty()) {
        std::error_code pathEc;
        const fs::path resolvedBase = fs::weakly_canonical(tempBase, pathEc);
        const fs::path resolvedDir = fs::weakly_canonical(tempDir, pathEc);
        if (!pathEc && resolvedDir.parent_path() == resolvedBase &&
            resolvedDir.filename() == std::to_string(runId)) {
            fs::remove_all(resolvedDir, ec);
        } else {
            check(false, "synthetic fixture cleanup path remains inside its TEMP root");
        }
    } else {
        std::cout << "PRESERVED_RENDER_OUTPUT_DIR=" << tempDir.string() << "\n";
    }
    diag.setEnabled(false);
    if (failures) {
        std::cerr << failures << " failure(s)\n";
        return 1;
    }
    std::cout << "All processed file export checks passed.\n";
    return 0;
}

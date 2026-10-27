#include "MediaFoundationAudioDecoder.h"
#include "platform/text_encoding.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#ifndef AURO_TEST_SOURCE_ROOT
#define AURO_TEST_SOURCE_ROOT ""
#endif

static int g_pass = 0;
static int g_fail = 0;

static void check(bool condition, const char* message) {
    if (condition) {
        std::printf("PASS: %s\n", message);
        ++g_pass;
    } else {
        std::printf("FAIL: %s\n", message);
        ++g_fail;
    }
}

static std::string readTextFile(const std::vector<const char*>& candidates) {
    for (const char* path : candidates) {
        if (AURO_TEST_SOURCE_ROOT[0] != '\0') {
            const std::filesystem::path rooted =
                std::filesystem::path(AURO_TEST_SOURCE_ROOT) / path;
            std::ifstream rootedInput(rooted, std::ios::binary);
            if (rootedInput.is_open()) {
                return std::string((std::istreambuf_iterator<char>(rootedInput)),
                                   std::istreambuf_iterator<char>());
            }
        }
        std::ifstream in(path, std::ios::binary);
        if (!in.is_open()) continue;
        return std::string((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
    }
    return {};
}

static void checkSourceContracts() {
    const std::string database = readTextFile({
        "src/Database.cpp", "../src/Database.cpp", "../../src/Database.cpp"});
    const std::string databaseProbeRouting = readTextFile({
        "src/platform/database_audio_probe.cpp",
        "../src/platform/database_audio_probe.cpp",
        "../../src/platform/database_audio_probe.cpp"});
    const std::string audio = readTextFile({
        "src/AudioEngine.cpp", "../src/AudioEngine.cpp", "../../src/AudioEngine.cpp"});
    const std::string waveform = readTextFile({
        "src/WaveformExtractor.cpp", "../src/WaveformExtractor.cpp", "../../src/WaveformExtractor.cpp"});
    const std::string fileBrowser = readTextFile({
        "src/FileBrowser.cpp", "../src/FileBrowser.cpp", "../../src/FileBrowser.cpp"});
    const std::string utils = readTextFile({
        "src/Utils.cpp", "../src/Utils.cpp", "../../src/Utils.cpp"});

    check(!database.empty(), "Database.cpp is readable");
    check(!databaseProbeRouting.empty(), "database audio probe routing is readable");
    check(!audio.empty(), "AudioEngine.cpp is readable");
    check(!waveform.empty(), "WaveformExtractor.cpp is readable");
    check(fileBrowser.find(".wma") != std::string::npos,
          "TDD-WMA-001: file browser whitelist still admits .wma");
    check(fileBrowser.find(".m4a") != std::string::npos,
          "TDD-M4A-001: file browser whitelist still admits .m4a");
    check(utils.find(".wma") != std::string::npos,
          "TDD-WMA-001: IsAudioFileSupported still admits .wma");
    check(utils.find(".m4a") != std::string::npos,
          "TDD-M4A-001: IsAudioFileSupported still admits .m4a");

    check(databaseProbeRouting.find("Detail::ProbeMediaFoundation") != std::string::npos,
          "TDD-MF-002: database probe routing owns Media Foundation probing");
    check(databaseProbeRouting.find("MediaFoundationProbeFailed") != std::string::npos &&
              databaseProbeRouting.find("fallback=flac") != std::string::npos,
          "BDD-MF-004: database probe routing preserves Media Foundation failure diagnostics");

    check(audio.find("MediaFoundationLoadAsyncDecodedPCMReady") != std::string::npos,
          "TDD-MF-003: async playback load has decoded PCM Media Foundation success path");
    check(audio.find("applyDecodedPcmLoadFileResult") != std::string::npos,
          "TDD-MF-003: decoded PCM apply path is separated from miniaudio decoder apply");
    check(audio.find("MediaFoundationBuildWaveformOK") != std::string::npos,
          "TDD-MF-004: main waveform has Media Foundation decode contract");
    check(audio.find("HarkenSearch::MediaFoundationAudio::CanDecodeExtension(path)") != std::string::npos &&
              audio.find("MediaFoundationMainDetailExtracted") != std::string::npos,
          "TDD-MF-006: playback main waveform bypasses miniaudio for Media Foundation detail");

    check(waveform.find("MediaFoundationMiniDecodeOK") != std::string::npos,
          "TDD-MF-004: mini waveform has Media Foundation decode contract");
    check(waveform.find("MediaFoundationDetailDecodeOK") != std::string::npos,
          "TDD-MF-004: detail waveform has Media Foundation decode contract");
    check(waveform.find("ClassifyFailure") != std::string::npos,
          "BDD-WMA-004: waveform WMA failures are classified instead of generic retry noise");
}

static void checkNegativeProbeContract() {
    const std::filesystem::path temp =
        std::filesystem::temp_directory_path() / "auro_invalid_probe_contract.wma";
    {
        std::ofstream out(temp, std::ios::binary);
        out << "not a real wma";
    }

    uint32_t sampleRate = 0;
    uint32_t channels = 0;
    uint64_t frames = 0;
    float duration = 0.0f;
    std::string error;
    const bool ok = HarkenSearch::MediaFoundationAudio::Probe(
        temp.string(), sampleRate, channels, frames, duration, &error);
    std::error_code ec;
    std::filesystem::remove(temp, ec);

    check(!ok, "TDD-WMA-005: invalid WMA probe fails without crashing");
    check(error.find("wma_media_foundation_unsupported") != std::string::npos ||
              error.find("media_foundation_unavailable") != std::string::npos,
          "TDD-WMA-005: invalid WMA probe returns classified failure");
}

struct PeakResult {
    std::vector<float> peaks;
    std::vector<float> minPeaks;
    std::vector<float> rms;
    int channels = 0;
    int samplesPerPeak = 0;
    uint64_t decodedFrames = 0;
    uint32_t sampleRate = 0;
    float trueRmsDbfs = -100.0f;
    bool hasTrueRms = false;
    bool ok = false;
    std::string errorCode;
};

static void checkPublicHeaderContract() {
    using namespace HarkenSearch::MediaFoundationAudio;
    const std::vector<std::pair<std::string, std::string>> formats = {
        {".wma", "WMA"}, {".flac", "FLAC"}, {".m4a", "M4A"},
        {".aac", "AAC"}, {".mp4", "MP4"}, {".m4p", "M4P"},
    };
    for (const auto& format : formats) {
        check(CanDecodeExtension(u8"C:\\Audio 音效 U0001F600\\CLIP" + format.first),
              "TDD-MF-PUBLIC-001: supported extension remains enabled");
        check(FormatNameFromPath("clip" + format.first) == format.second,
              "TDD-MF-PUBLIC-002: supported format name remains stable");
    }
    check(CanDecodeExtension(u8"C:\\Audio 音效 U0001F600\\CLIP.WMA"),
          "TDD-MF-PUBLIC-003: Unicode path and uppercase extension are supported");
    check(!CanDecodeExtension("folder.with.dot/no_extension"),
          "TDD-MF-PUBLIC-004: a directory dot is not an audio extension");
    check(std::string(FormatNameFromPath("clip.unknown")) == "MediaFoundation",
          "TDD-MF-PUBLIC-005: unknown format keeps fallback label");
    check(ClassifyFailure("") == "wma_media_foundation_unsupported",
          "TDD-MF-PUBLIC-006: empty failure keeps unsupported classification");
    check(ClassifyFailure("DRM blocked").find("wma_media_foundation_drm_or_protected") == 0,
          "TDD-MF-PUBLIC-007: DRM failure remains distinct");
    check(ClassifyFailure("unexpected").find("wma_media_foundation_unsupported:") == 0,
          "TDD-MF-PUBLIC-008: unknown failure keeps unsupported prefix");

    uint32_t sampleRate = 1;
    uint32_t channels = 2;
    uint64_t totalFrames = 3;
    float duration = 4.0f;
    std::string error = "stale";
    check(!Probe("unsupported.wav", sampleRate, channels, totalFrames, duration, &error) &&
              sampleRate == 0 && channels == 0 && totalFrames == 0 && duration == 0.0f &&
              error == "media_foundation_extension_not_supported",
          "TDD-MF-PUBLIC-009: unsupported probe resets outputs before failing");

    DecodedAudioBuffer decoded;
    check(decoded.sampleRate == 0 && decoded.channels == 0 &&
              decoded.totalFrames == 0 && decoded.durationSeconds == 0.0f &&
              decoded.interleaved.empty(),
          "TDD-MF-PUBLIC-010: decoded buffer defaults remain empty");
    decoded.sampleRate = 1;
    decoded.interleaved = {1.0f};
    error = "stale";
    check(!Decode("unsupported.wav", decoded, &error) && decoded.sampleRate == 0 &&
              decoded.interleaved.empty() &&
              error == "media_foundation_extension_not_supported",
          "TDD-MF-PUBLIC-011: unsupported decode resets output before failing");
    decoded.sampleRate = 48000;
    decoded.channels = 2;
    decoded.totalFrames = 2;
    decoded.interleaved = {0.5f, -0.25f, -0.75f, 1.0f};
    PeakResult peaks;
    check(BuildPeaksFromDecoded(decoded, 2, true, true, peaks),
          "TDD-MF-PUBLIC-012: in-memory decoded PCM builds peaks");
    check(peaks.channels == 2 && peaks.sampleRate == 48000 &&
              peaks.decodedFrames == 2 && peaks.peaks.size() == 4 &&
              peaks.peaks[0] == 0.5f && peaks.minPeaks[1] == -0.25f &&
              peaks.minPeaks[2] == -0.75f && peaks.peaks[3] == 1.0f &&
              peaks.hasTrueRms,
          "TDD-MF-PUBLIC-013: peak shape and signed extrema remain stable");
    check(!BuildPeaksFromDecoded(decoded, 0, false, false, peaks) &&
              peaks.errorCode == "wma_decode_empty",
          "TDD-MF-PUBLIC-014: invalid peak request resets to empty failure");
}

#ifdef _WIN32
static void checkWindowsPathContract() {
    using HarkenSearch::MediaFoundationAudio::detail::ToMediaFoundationPath;
    using Platform::TextEncoding::Utf8ToWidePermissive;

    const std::string unicodePath =
        u8"C:\\Audio Library\\\u97F3\u6548 \U0001F600\\wide space.wma";
    check(Utf8ToWidePermissive(unicodePath) ==
              L"C:\\Audio Library\\\u97F3\u6548 \U0001F600\\wide space.wma",
          "TDD-MF-PATH-001: CJK emoji and spaces convert exactly");

    const std::string malformed("\xF0\x28\x8C\x28", 4);
    check(Utf8ToWidePermissive(malformed) == L"\uFFFD(\uFFFD(",
          "TDD-MF-PATH-002: malformed UTF-8 converts with replacement");
    check(Utf8ToWidePermissive("").empty(),
          "TDD-MF-PATH-003: empty UTF-8 remains empty");
    const char embedded[] = "C:\\valid.wma\0shadow.wma";
    check(Utf8ToWidePermissive(
              std::string(embedded, sizeof(embedded) - 1u)).empty(),
          "TDD-MF-PATH-004: embedded NUL fails closed");

    check(ToMediaFoundationPath(L"\\\\?\\C:\\Audio/one.wma") ==
              L"C:\\Audio\\one.wma",
          "TDD-MF-PATH-005: drive long-path prefix is removed");
    check(ToMediaFoundationPath(L"\\\\?\\UNC\\server\\share/fx.wma") ==
              L"\\\\server\\share\\fx.wma",
          "TDD-MF-PATH-006: UNC long-path prefix maps to network path");
    const std::wstring longTail(400, L'x');
    check(ToMediaFoundationPath(L"\\\\?\\C:\\" + longTail + L"/fx.wma") ==
              L"C:\\" + longTail + L"\\fx.wma",
          "TDD-MF-PATH-007: long path normalizes without truncation");
}
#endif

int main() {
    std::printf("=== TestWmaDecodeSupport ===\n");
    check(HarkenSearch::MediaFoundationAudio::HasWmaExtension("song.wma"),
          "TDD-WMA-001: helper recognizes .wma");
    check(HarkenSearch::MediaFoundationAudio::HasWmaExtension("SONG.WMA"),
          "TDD-WMA-001: helper recognizes uppercase .WMA");
    check(!HarkenSearch::MediaFoundationAudio::HasWmaExtension("song.wav"),
          "TDD-WMA-001: helper does not hijack existing formats");
    check(HarkenSearch::MediaFoundationAudio::CanDecodeExtension("song.m4a"),
          "TDD-M4A-001: Media Foundation helper recognizes .m4a");
    check(HarkenSearch::MediaFoundationAudio::CanDecodeExtension("song.M4A"),
          "TDD-M4A-001: Media Foundation helper recognizes uppercase .M4A");
    check(HarkenSearch::MediaFoundationAudio::CanDecodeExtension("wrapped_flac.flac"),
          "TDD-FLAC-008: Media Foundation helper recognizes .flac fallback containers");
    check(!HarkenSearch::MediaFoundationAudio::CanDecodeExtension("song.wav"),
          "TDD-MF-001: Media Foundation helper does not hijack WAV");
    check(HarkenSearch::MediaFoundationAudio::ClassifyFailure(
              "wma_media_foundation_codec_missing_or_unsupported:hr=0xC00D36C4") ==
              "wma_media_foundation_codec_missing_or_unsupported:hr=0xC00D36C4",
          "TDD-WMA-005: classified WMA failures are idempotent");
    check(HarkenSearch::MediaFoundationAudio::ClassifyFailure("hr=0xC00D36C4").find(
              "wma_media_foundation_codec_missing_or_unsupported") == 0,
          "TDD-WMA-005: codec-missing HRESULT receives actionable classification");
    checkPublicHeaderContract();
    checkSourceContracts();
#ifdef _WIN32
    checkWindowsPathContract();
#endif
    checkNegativeProbeContract();
    std::printf("\nResults: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}

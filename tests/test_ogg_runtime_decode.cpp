#include "WaveformExtractor.h"
#include "MediaFoundationAudioDecoder.h"

#include "../thirdparty/miniaudio.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

#ifdef _WIN32
#include <windows.h>
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

static bool commandExists(const char* command) {
#ifdef _WIN32
    std::string probe = "where ";
    probe += command;
    probe += " >nul 2>nul";
#else
    std::string probe = "command -v ";
    probe += command;
    probe += " >/dev/null 2>/dev/null";
#endif
    return std::system(probe.c_str()) == 0;
}

static bool generateFixture(const std::filesystem::path& path,
                            const char* frequency,
                            const char* codecArgs) {
    std::string command = "ffmpeg -hide_banner -loglevel error -y "
        "-f lavfi -i \"sine=frequency=";
    command += frequency;
    command += ":duration=0.1\" ";
    if (codecArgs != nullptr && codecArgs[0] != '\0') {
        command += codecArgs;
        command += " ";
    }
    command += "\"";
    command += path.string();
    command += "\"";

    return std::system(command.c_str()) == 0 && std::filesystem::exists(path);
}

static std::string readEnvString(const char* name) {
#ifdef _WIN32
    char* raw = nullptr;
    size_t rawLen = 0;
    if (_dupenv_s(&raw, &rawLen, name) != 0 || raw == nullptr) {
        return {};
    }
    std::string value = raw;
    std::free(raw);
    return value;
#else
    const char* raw = std::getenv(name);
    return raw ? std::string(raw) : std::string();
#endif
}

static void checkWaveformExtraction(const std::filesystem::path& path,
                                    const char* miniMessage,
                                    const char* miniPeaksMessage,
                                    const char* detailMessage,
                                    const char* detailPeaksMessage) {
    WaveformExtractResult mini;
    const bool miniOk = WaveformExtractor::ExtractMini(path.string(), 160, mini, false);
    if (!miniOk) {
        std::printf("INFO: mini errorCode=%s\n", mini.errorCode.c_str());
    }
    check(miniOk, miniMessage);
    check(!mini.peaks.empty(), miniPeaksMessage);

    WaveformExtractResult detail;
    const bool detailOk = WaveformExtractor::ExtractDetail(path.string(), 1024, true, detail);
    if (!detailOk) {
        std::printf("INFO: detail errorCode=%s\n", detail.errorCode.c_str());
    }
    check(detailOk, detailMessage);
    check(!detail.peaks.empty(), detailPeaksMessage);
}

int main() {
    std::printf("=== TestOggRuntimeDecode ===\n");

    if (!commandExists("ffmpeg")) {
        std::printf("SKIP: ffmpeg unavailable; runtime OGG fixture generation skipped\n");
        return 0;
    }

    const std::filesystem::path oggPath =
        std::filesystem::temp_directory_path() / "auro_ogg_runtime_decode_test.ogg";
    const std::filesystem::path aiffPath =
        std::filesystem::temp_directory_path() / "auro_aiff_runtime_decode_test.aiff";
    const std::filesystem::path flacPath =
        std::filesystem::temp_directory_path() / "auro_flac_runtime_decode_test.flac";
    const std::filesystem::path wmaPath =
        std::filesystem::temp_directory_path() / "auro_wma_runtime_decode_test.wma";
    const std::filesystem::path m4aPath =
        std::filesystem::temp_directory_path() / "auro_m4a_runtime_decode_test.m4a";

    const bool oggGenerated = generateFixture(oggPath, "440", "-c:a libvorbis");
    check(oggGenerated,
          "TDD-OGG-006: ffmpeg generated a valid OGG/Vorbis fixture");
    if (!oggGenerated) {
        return 1;
    }

    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 2, 48000);
    ma_decoder decoder;
#ifdef _WIN32
    ma_result initResult = ma_decoder_init_file_w(oggPath.wstring().c_str(), &config, &decoder);
#else
    ma_result initResult = ma_decoder_init_file(oggPath.string().c_str(), &config, &decoder);
#endif
    check(initResult == MA_SUCCESS,
          "TDD-OGG-006: miniaudio initializes OGG/Vorbis decoder");

    if (initResult == MA_SUCCESS) {
        float frames[480 * 2] = {};
        ma_uint64 framesRead = 0;
        ma_result readResult = ma_decoder_read_pcm_frames(&decoder, frames, 480, &framesRead);
        check(readResult == MA_SUCCESS || readResult == MA_AT_END,
              "TDD-OGG-006: OGG/Vorbis decoder reads PCM frames");
        check(framesRead > 0,
              "TDD-OGG-006: OGG/Vorbis decode produces non-empty PCM");
        ma_decoder_uninit(&decoder);
    }

    checkWaveformExtraction(oggPath,
        "TDD-OGG-007: WaveformExtractor builds OGG mini waveform",
        "TDD-OGG-007: OGG mini waveform has visible peaks",
        "TDD-OGG-008: WaveformExtractor builds OGG main detail waveform",
        "TDD-OGG-008: OGG main detail waveform has visible peaks");

    check(generateFixture(aiffPath, "660", ""),
          "TDD-AIFF-001: ffmpeg generated a valid AIFF fixture");
    if (std::filesystem::exists(aiffPath)) {
        checkWaveformExtraction(aiffPath,
            "TDD-AIFF-002: WaveformExtractor builds AIFF mini waveform",
            "TDD-AIFF-002: AIFF mini waveform has visible peaks",
            "TDD-AIFF-003: WaveformExtractor builds AIFF main detail waveform",
            "TDD-AIFF-003: AIFF main detail waveform has visible peaks");
    }

    check(generateFixture(flacPath, "770", "-c:a flac"),
          "TDD-FLAC-001: ffmpeg generated a valid FLAC fixture");
    if (std::filesystem::exists(flacPath)) {
        ma_decoder_config flacConfig = ma_decoder_config_init(ma_format_f32, 2, 48000);
        ma_decoder flacDecoder;
#ifdef _WIN32
        ma_result flacInit = ma_decoder_init_file_w(flacPath.wstring().c_str(), &flacConfig, &flacDecoder);
#else
        ma_result flacInit = ma_decoder_init_file(flacPath.string().c_str(), &flacConfig, &flacDecoder);
#endif
        check(flacInit == MA_SUCCESS,
              "TDD-FLAC-002: miniaudio initializes FLAC decoder");
        if (flacInit == MA_SUCCESS) {
            float frames[480 * 2] = {};
            ma_uint64 framesRead = 0;
            ma_result readResult = ma_decoder_read_pcm_frames(&flacDecoder, frames, 480, &framesRead);
            check(readResult == MA_SUCCESS || readResult == MA_AT_END,
                  "TDD-FLAC-002: FLAC decoder reads PCM frames");
            check(framesRead > 0,
                  "TDD-FLAC-002: FLAC decode produces non-empty PCM");
            ma_decoder_uninit(&flacDecoder);
        }

        checkWaveformExtraction(flacPath,
            "TDD-FLAC-003: WaveformExtractor builds FLAC mini waveform",
            "TDD-FLAC-003: FLAC mini waveform has visible peaks",
            "TDD-FLAC-004: WaveformExtractor builds FLAC main detail waveform",
            "TDD-FLAC-004: FLAC main detail waveform has visible peaks");
    }

    const std::string externalFlac = readEnvString("AURO_FLAC_PROBE_PATH");
    if (!externalFlac.empty()) {
        const std::filesystem::path externalFlacPath = externalFlac;
        if (std::filesystem::exists(externalFlacPath)) {
            uint32_t sr = 0;
            uint32_t ch = 0;
            uint64_t frames = 0;
            float duration = 0.0f;
            std::string probeError;
            const bool probed = HarkenSearch::MediaFoundationAudio::Probe(
                externalFlacPath.string(), sr, ch, frames, duration, &probeError);
            if (!probed) {
                std::printf("INFO: external FLAC MediaFoundation probe error=%s\n", probeError.c_str());
            }
            check(probed,
                  "TDD-FLAC-007: Media Foundation probes external MP4-container FLAC metadata");
            check(sr > 0 && ch > 0 && frames > 0 && duration > 0.0f,
                  "TDD-FLAC-007: external MP4-container FLAC metadata has duration/sample-rate/channels");
            checkWaveformExtraction(externalFlacPath,
                "TDD-FLAC-005: WaveformExtractor builds external FLAC mini waveform",
                "TDD-FLAC-005: external FLAC mini waveform has visible peaks",
                "TDD-FLAC-006: WaveformExtractor builds external FLAC main detail waveform",
                "TDD-FLAC-006: external FLAC main detail waveform has visible peaks");
        } else {
            std::printf("INFO: AURO_FLAC_PROBE_PATH does not exist: %s\n", externalFlac.c_str());
        }
    }

    check(generateFixture(wmaPath, "880", "-c:a wmav2"),
          "TDD-WMA-006: ffmpeg generated a valid WMA fixture");
    if (std::filesystem::exists(wmaPath)) {
        checkWaveformExtraction(wmaPath,
            "TDD-WMA-007: WaveformExtractor builds WMA mini waveform",
            "TDD-WMA-007: WMA mini waveform has visible peaks",
            "TDD-WMA-008: WaveformExtractor builds WMA main detail waveform",
            "TDD-WMA-008: WMA main detail waveform has visible peaks");
    }

    check(generateFixture(m4aPath, "990", "-c:a aac -b:a 96k"),
          "TDD-M4A-006: ffmpeg generated a valid M4A/AAC fixture");
    if (std::filesystem::exists(m4aPath)) {
        HarkenSearch::MediaFoundationAudio::DecodedAudioBuffer decodedM4a;
        std::string m4aDecodeError;
        const bool m4aDecoded = HarkenSearch::MediaFoundationAudio::Decode(
            m4aPath.string(), decodedM4a, &m4aDecodeError);
        if (!m4aDecoded) {
            std::printf("INFO: M4A MediaFoundation decode error=%s\n", m4aDecodeError.c_str());
        }
        check(m4aDecoded,
              "TDD-M4A-009: Media Foundation decodes M4A playback PCM");
        check(!decodedM4a.interleaved.empty() && decodedM4a.totalFrames > 0,
              "TDD-M4A-009: M4A playback PCM has frames");
        checkWaveformExtraction(m4aPath,
            "TDD-M4A-007: WaveformExtractor builds M4A mini waveform",
            "TDD-M4A-007: M4A mini waveform has visible peaks",
            "TDD-M4A-008: WaveformExtractor builds M4A main detail waveform",
            "TDD-M4A-008: M4A main detail waveform has visible peaks");
    }

    std::error_code ec;
    std::filesystem::remove(oggPath, ec);
    std::filesystem::remove(aiffPath, ec);
    std::filesystem::remove(flacPath, ec);
    std::filesystem::remove(wmaPath, ec);
    std::filesystem::remove(m4aPath, ec);

    std::printf("\nResults: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}

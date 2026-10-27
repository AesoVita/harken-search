#include "AudioEngine.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

static int failed = 0;
static int passed = 0;

static void CHECK(bool condition, const char* detail) {
    if (condition) {
        ++passed;
        std::printf("  PASS: %s\n", detail);
    } else {
        ++failed;
        std::printf("  FAIL: %s\n", detail);
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

static bool generateVorbisFixture(const std::filesystem::path& path) {
    std::string command = "ffmpeg -hide_banner -loglevel error -y "
        "-f lavfi -i \"sine=frequency=1000:duration=1\" -c:a libvorbis \"";
    command += path.string();
    command += "\"";
    return std::system(command.c_str()) == 0 && std::filesystem::exists(path);
}

static bool isFiniteLoudness(float value) {
    return std::isfinite(value);
}

int main() {
    std::printf("=== OggLoudnessMeter Tests ===\n");

    if (!commandExists("ffmpeg")) {
        std::printf("  FAIL: ffmpeg unavailable; real OGG/Vorbis fixture required\n");
        return 1;
    }

    const std::filesystem::path oggPath =
        std::filesystem::temp_directory_path() / "aris_ogg_loudness_meter_test.ogg";
    std::error_code removeError;
    std::filesystem::remove(oggPath, removeError);

    const bool generated = generateVorbisFixture(oggPath);
    CHECK(generated, "ffmpeg generated a real OGG/Vorbis fixture");
    if (!generated) {
        std::printf("\n=== OggLoudnessMeter: %d passed, %d failed ===\n", passed, failed);
        return 1;
    }

    WaveformData waveform;
    const bool ok = AudioEngine::generateWaveformForFile(oggPath.string(), waveform, 100);
    CHECK(ok, "generateWaveformForFile succeeds on real OGG/Vorbis");
    CHECK(ok && waveform.hasLoudnessData, "OGG generateWaveformForFile sets hasLoudnessData");
    CHECK(ok && isFiniteLoudness(waveform.integratedLUFS), "integratedLUFS is finite");
    CHECK(ok && isFiniteLoudness(waveform.trueRmsDbfs), "trueRmsDbfs is finite");
    CHECK(ok && isFiniteLoudness(waveform.truePeakDBTP), "truePeakDBTP is finite");
    CHECK(ok && waveform.hasTrueRms, "OGG generateWaveformForFile sets hasTrueRms");

    std::filesystem::remove(oggPath, removeError);
    std::printf("\n=== OggLoudnessMeter: %d passed, %d failed ===\n", passed, failed);
    return failed ? 1 : 0;
}

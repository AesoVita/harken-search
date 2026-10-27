#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

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

static std::string readTextFile(const std::filesystem::path& relativePath) {
    const auto repoRoot = std::filesystem::path(__FILE__).parent_path().parent_path();
    std::ifstream in(repoRoot / relativePath, std::ios::binary);
    if (!in) return {};
    return std::string((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
}

int main() {
    std::printf("=== TestOggPlaybackSupport ===\n");

    const std::string audio = readTextFile("src/AudioEngine.cpp");
    const std::string miniaudioImpl = readTextFile("tests/miniaudio_impl.cpp");
    const std::string database = readTextFile("src/Database.cpp");
    const std::string databaseProbeBackend = readTextFile("src/platform/database_audio_probe_miniaudio.cpp");
    const std::string mainUi = readTextFile("src/main.cpp");
    const std::string windowsMain = readTextFile("src/WindowsMain.cpp");

    check(!audio.empty(), "AudioEngine.cpp is readable");
    check(!miniaudioImpl.empty(), "tests/miniaudio_impl.cpp is readable");
    check(!database.empty(), "Database.cpp is readable");
    check(!databaseProbeBackend.empty(), "database audio probe backend is readable");
    check(!mainUi.empty(), "main.cpp is readable");
    check(!windowsMain.empty(), "WindowsMain.cpp is readable");

    check(audio.find("STB_VORBIS_HEADER_ONLY") < audio.find("MINIAUDIO_IMPLEMENTATION"),
          "TDD-OGG-001: AudioEngine enables stb_vorbis before miniaudio implementation");
    check(audio.find("#undef STB_VORBIS_HEADER_ONLY") != std::string::npos,
          "TDD-OGG-001: AudioEngine emits stb_vorbis implementation after miniaudio");
    check(miniaudioImpl.find("STB_VORBIS_HEADER_ONLY") < miniaudioImpl.find("MINIAUDIO_IMPLEMENTATION"),
          "TDD-OGG-001: standalone miniaudio test implementation also enables stb_vorbis");

    check(audio.find("OGGLoadDecodeReady") != std::string::npos,
          "BDD-OGG-002: async playback success has explicit OGG diagnostic");
    check(audio.find("OGGLoadDecodeFailed") != std::string::npos,
          "BDD-OGG-003: async playback failure has explicit OGG diagnostic");

    check(database.find(".ogg") != std::string::npos,
          "TDD-OGG-004: database scan whitelist still admits .ogg");
    check(database.find("probeAudioFileInfo(") != std::string::npos,
          "TDD-OGG-009: Database exposes a shared file-info probe for scan/import/rescan paths");
    check(databaseProbeBackend.find("if (length == 0)") != std::string::npos &&
              databaseProbeBackend.find("kChunkFrames = 8192") != std::string::npos &&
              databaseProbeBackend.find("ReadFrames(") != std::string::npos,
          "TDD-OGG-010: database probe backend preserves zero-length sequential decode fallback");
    check(mainUi.find("\"OGG\"") != std::string::npos,
          "TDD-OGG-004: UI format filter still exposes OGG");
    check(mainUi.find("return state_.database.probeAudioFileInfo(") != std::string::npos,
          "TDD-OGG-011: rescan path reuses the shared fallback-safe audio file-info probe");
    check(windowsMain.find("g_appState->database.probeAudioFileInfo(filePath") != std::string::npos,
          "TDD-OGG-012: Windows import path reuses the shared fallback-safe audio file-info probe");
    check(windowsMain.find("\"load_path\"") != std::string::npos &&
              windowsMain.find("loadFileAsync(path") != std::string::npos,
          "TDD-OGG-005: QA load_path command reaches playback load path");

    std::printf("\nResults: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}

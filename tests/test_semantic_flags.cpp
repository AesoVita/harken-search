// test_semantic_flags.cpp — Verifies current production semantic flags keep
// their required values. Catches accidental flag flips across AI sessions,
// but should only guard flags that still exist in the active runtime contract.
//
// Each CHECK_FLAG reads the actual source file and verifies the flag's value.
// If a flag was changed for debugging and accidentally committed, this test fails.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>

static int passed = 0;
static int failed = 0;

static void CHECK(bool cond, const char* desc) {
    if (cond) { ++passed; std::printf("  PASS: %s\n", desc); }
    else      { ++failed; std::printf("  FAIL: %s\n", desc); }
}

// Search a source file for a line matching "constexpr bool <name> = <value>;"
static bool verifyFlag(const char* filePath, const char* flagName, const char* expectedValue) {
    std::ifstream f(filePath);
    if (!f.is_open()) {
        std::fprintf(stderr, "  ERROR: cannot open %s\n", filePath);
        return false;
    }
    std::string line;
    std::string needle = std::string("constexpr bool ") + flagName;
    while (std::getline(f, line)) {
        if (line.find(needle) != std::string::npos) {
            std::string valNeedle = std::string("= ") + expectedValue + ";";
            return line.find(valNeedle) != std::string::npos;
        }
    }
    std::fprintf(stderr, "  ERROR: flag '%s' not found in %s\n", flagName, filePath);
    return false;
}

// Search a source file for "constexpr int <name> = <N>;" and verify N >= minValue
static bool verifyIntMin(const char* filePath, const char* flagName, int minValue) {
    std::ifstream f(filePath);
    if (!f.is_open()) {
        std::fprintf(stderr, "  ERROR: cannot open %s\n", filePath);
        return false;
    }
    std::string line;
    std::string needle = std::string(flagName);
    while (std::getline(f, line)) {
        if (line.find(needle) == std::string::npos) continue;
        if (line.find("constexpr int") == std::string::npos) continue;
        auto eqPos = line.find('=');
        if (eqPos == std::string::npos) continue;
        int val = std::atoi(line.c_str() + eqPos + 1);
        if (val >= minValue) return true;
        std::fprintf(stderr, "  ERROR: %s = %d, expected >= %d\n", flagName, val, minValue);
        return false;
    }
    std::fprintf(stderr, "  ERROR: flag '%s' not found in %s\n", flagName, filePath);
    return false;
}

static std::string readFile(const char* filePath) {
    std::ifstream f(filePath, std::ios::binary);
    if (!f.is_open()) {
        std::fprintf(stderr, "  ERROR: cannot open %s\n", filePath);
        return {};
    }
    return std::string((std::istreambuf_iterator<char>(f)),
                       std::istreambuf_iterator<char>());
}

static bool fileContains(const char* filePath, const char* needle) {
    const std::string text = readFile(filePath);
    return !text.empty() && text.find(needle) != std::string::npos;
}

int main() {
    std::printf("=== Semantic Flag Tests ===\n");
    std::printf("  Verifies current runtime semantic flags haven't been accidentally changed.\n\n");

    // ── File paths ──
#ifdef AURA_SOURCE_DIR
    const char* ae  = AURA_SOURCE_DIR "/src/AudioEngine.cpp";
    const char* mc  = AURA_SOURCE_DIR "/src/main.cpp";
    const char* wm  = AURA_SOURCE_DIR "/src/WindowsMain.cpp";
    const char* db  = AURA_SOURCE_DIR "/src/Database.cpp";
    const char* weh = AURA_SOURCE_DIR "/src/WaveformExtractor.h";
#else
    const char* ae  = "src/AudioEngine.cpp";
    const char* mc  = "src/main.cpp";
    const char* wm  = "src/WindowsMain.cpp";
    const char* db  = "src/Database.cpp";
    const char* weh = "src/WaveformExtractor.h";
#endif

    std::printf("--- AudioEngine flags ---\n");

    CHECK(verifyFlag(ae, "kEnableWaveformVisualStretchRepair", "true"),
          "SEMANTIC: kEnableWaveformVisualStretchRepair must be true — "
          "drives stream-EOF (TruncatedAudioMetadata) stretch for BWF FACT-mismatch files");

    CHECK(verifyFlag(ae, "kEnableHeuristicSilenceStretch", "false"),
          "SEMANTIC: kEnableHeuristicSilenceStretch must be false — "
          "guards the cache-hit and post-decode heuristic-stretch sites that "
          "mistake legit tail silence for FACT-truncation and visually 'double' "
          "SFX waveforms (2026-05-20 BoomLibrary regression)");

    CHECK(verifyFlag(ae, "kEnableLufsDetection", "true"),
          "SEMANTIC: kEnableLufsDetection must be true — "
          "disabling removes all loudness metering");

    CHECK(verifyFlag(ae, "kEnableJitWaveformFallback", "true"),
          "SEMANTIC: kEnableJitWaveformFallback must be true — "
          "disabling breaks waveform display for uncached files");

    CHECK(verifyFlag(ae, "kEnableProgressiveMainWavePublish", "true"),
          "SEMANTIC: kEnableProgressiveMainWavePublish must be true — "
          "disabling prevents incremental waveform rendering");

    CHECK(verifyFlag(ae, "kAllowMainWaveSeekLod", "false"),
          "SEMANTIC: kAllowMainWaveSeekLod must be false — "
          "enabling shows low-quality seek LOD as final waveform");

    CHECK(verifyFlag(ae, "kAotWaveformCacheOnly", "false"),
          "SEMANTIC: kAotWaveformCacheOnly must be false — "
          "enabling disables JIT waveform generation for uncached files");

    // ── main.cpp flags ──
    std::printf("\n--- main.cpp flags ---\n");

    CHECK(verifyFlag(mc, "kEnableLufsUiAndDetection", "true"),
          "SEMANTIC: kEnableLufsUiAndDetection must be true — "
          "disabling removes LUFS column and loudness meter from UI");

    // ── WindowsMain.cpp flags ──
    CHECK(fileContains(mc, "folderFilterFastFirstPaint"),
          "SEMANTIC: folder tree filtering keeps an explicit first-paint fast path");
    CHECK(fileContains(mc, "const bool deferCount = folderFilterFastFirstPaint"),
          "SEMANTIC: folder tree first paint defers count/facet follow-up work");

    std::printf("\n--- WindowsMain.cpp flags ---\n");

    CHECK(verifyFlag(wm, "kUseBetaSplash", "true"),
          "SEMANTIC: kUseBetaSplash must be true — "
          "disabling reverts to unfinished WebView2 login flow");

    // ── Database.cpp version constants ──
    std::printf("\n--- Database.cpp version constants ---\n");

    CHECK(verifyIntMin(db, "kWaveformCacheVersion", 8),
          "SEMANTIC: kWaveformCacheVersion must be >= 8 — "
          "v8 invalidates detail blobs poisoned by the post-decode silence-stretch "
          "site (commit 13ec9bc disabled the site but bad cached peaks persist "
          "until re-extract; lowering skips this critical cleanup)");

    // ── WaveformExtractor.h version constants ──
    CHECK(fileContains(db, "INDEXED BY idx_files_folder_name"),
          "SEMANTIC: folder tree search uses the folder/name index for first-page browsing");

    std::printf("\n--- WaveformExtractor.h version constants ---\n");

    CHECK(verifyIntMin(weh, "kExtractorVersion", 4),
          "SEMANTIC: kExtractorVersion must be >= 4 — "
          "lowering disables decoded-frame timeline in detail buckets");

    std::printf("\n=== Semantic Flags: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}

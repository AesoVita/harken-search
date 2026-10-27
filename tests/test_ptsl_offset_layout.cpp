#include "ProToolsBridge.h"

#include <cstdlib>
#include <iostream>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        std::exit(1);
    }
}

using namespace HarkenSearch::ProTools;

LivePtslSpotFile MakeFile(std::uint64_t duration, std::uint32_t srcSR,
                           std::uint64_t bwfTC = 0, std::uint16_t channels = 1) {
    LivePtslSpotFile f;
    f.stagedPathUtf8 = "dummy.wav";
    f.displayNameUtf8 = "dummy.wav";
    f.durationSamples = duration;
    f.sourceSampleRate = srcSR;
    f.bwfTimeReferenceSamples = bwfTC;
    f.channelCount = channels;
    return f;
}

// ── AtCursor ──────────────────────────────────────────────────────────────────

void TestAtCursor_SingleFile() {
    std::vector<LivePtslSpotFile> files = { MakeFile(48000, 48000) };
    const auto pos = ComputeSpotPositions(files, SpotMode::AtCursor, 1000, 48000);
    Require(pos.size() == 1, "AtCursor single: one position");
    Require(pos[0] == 1000, "AtCursor single: position equals base");
}

void TestAtCursor_MultiFile_SameSR() {
    // 3 files, 1s each at 48kHz, cursor at 0
    std::vector<LivePtslSpotFile> files = {
        MakeFile(48000, 48000),
        MakeFile(48000, 48000),
        MakeFile(48000, 48000),
    };
    const auto pos = ComputeSpotPositions(files, SpotMode::AtCursor, 0, 48000);
    Require(pos.size() == 3, "AtCursor multi: three positions");
    Require(pos[0] == 0,     "AtCursor multi: clip 0 at base");
    Require(pos[1] == 48000, "AtCursor multi: clip 1 starts after clip 0");
    Require(pos[2] == 96000, "AtCursor multi: clip 2 starts after clip 1");
}

void TestAtCursor_MultiFile_SRUpscale() {
    // Source 44.1kHz, session 48kHz; 1s source-file = 44100 source samples = 48000 session samples
    std::vector<LivePtslSpotFile> files = {
        MakeFile(44100, 44100),
        MakeFile(44100, 44100),
    };
    const auto pos = ComputeSpotPositions(files, SpotMode::AtCursor, 0, 48000);
    Require(pos.size() == 2, "SR upscale: two positions");
    Require(pos[0] == 0, "SR upscale: clip 0 at base");
    // 44100 * 48000 / 44100 = 48000 (exact — round-trip SR)
    Require(pos[1] == 48000, "SR upscale: clip 1 at 48000 session samples");
}

void TestAtCursor_MultiFile_SRDownscale() {
    // Source 48kHz, session 44.1kHz; 48000 source samples * 44100/48000 = 44100 session samples
    std::vector<LivePtslSpotFile> files = {
        MakeFile(48000, 48000),
        MakeFile(48000, 48000),
    };
    const auto pos = ComputeSpotPositions(files, SpotMode::AtCursor, 0, 44100);
    Require(pos.size() == 2, "SR downscale: two positions");
    Require(pos[0] == 0, "SR downscale: clip 0 at base");
    Require(pos[1] == 44100, "SR downscale: clip 1 at 44100 session samples");
}

void TestAtCursor_MissingDuration() {
    // durationSamples == 0 → clips pile on the same position
    std::vector<LivePtslSpotFile> files = {
        MakeFile(0, 48000),
        MakeFile(0, 48000),
    };
    const auto pos = ComputeSpotPositions(files, SpotMode::AtCursor, 500, 48000);
    Require(pos.size() == 2, "missing duration: two positions returned");
    Require(pos[0] == 500 && pos[1] == 500, "missing duration: all clips at base");
}

// ── ToSelectionIn ──────────────────────────────────────────────────────────────

void TestToSelectionIn_IsSameAsAtCursor() {
    std::vector<LivePtslSpotFile> files = {
        MakeFile(48000, 48000),
        MakeFile(24000, 48000),
    };
    const auto posAtCursor = ComputeSpotPositions(files, SpotMode::AtCursor, 1000, 48000);
    const auto posSelIn    = ComputeSpotPositions(files, SpotMode::ToSelectionIn, 1000, 48000);
    Require(posAtCursor == posSelIn, "ToSelectionIn layout is identical to AtCursor");
}

// ── ToTimecode ────────────────────────────────────────────────────────────────

void TestToTimecode_SameSR() {
    // BWF TC at exactly 2s from start, session 48kHz, source 48kHz → position = 96000
    std::vector<LivePtslSpotFile> files = { MakeFile(48000, 48000, 96000) };
    const auto pos = ComputeSpotPositions(files, SpotMode::ToTimecode, 0, 48000);
    Require(pos.size() == 1, "ToTimecode: one position");
    Require(pos[0] == 96000, "ToTimecode SameSR: position equals BWF TC");
}

void TestToTimecode_SRConvert() {
    // BWF TC = 44100 source samples (1s at 44.1kHz), session 48kHz → 48000
    std::vector<LivePtslSpotFile> files = { MakeFile(44100, 44100, 44100) };
    const auto pos = ComputeSpotPositions(files, SpotMode::ToTimecode, 999, 48000);
    Require(pos.size() == 1, "ToTimecode SR convert: one position");
    Require(pos[0] == 48000, "ToTimecode SR convert: BWF TC scaled to session SR");
}

void TestToTimecode_MultiFile_Independent() {
    // Two files at different TC positions — no sequential layout
    std::vector<LivePtslSpotFile> files = {
        MakeFile(48000, 48000, 48000),   // BWF TC = 1s
        MakeFile(48000, 48000, 240000),  // BWF TC = 5s
    };
    const auto pos = ComputeSpotPositions(files, SpotMode::ToTimecode, 0, 48000);
    Require(pos.size() == 2, "ToTimecode multi: two positions");
    Require(pos[0] == 48000,  "ToTimecode multi: clip 0 at its BWF TC");
    Require(pos[1] == 240000, "ToTimecode multi: clip 1 at its BWF TC (independent)");
}

void TestToTimecode_MissingBWF_FallsBackToBase() {
    // bwfTimeReferenceSamples == 0 → should fall back to basePosition
    std::vector<LivePtslSpotFile> files = { MakeFile(48000, 48000, 0) };
    const auto pos = ComputeSpotPositions(files, SpotMode::ToTimecode, 12345, 48000);
    Require(pos.size() == 1, "ToTimecode missing BWF: one position");
    Require(pos[0] == 12345, "ToTimecode missing BWF: falls back to base cursor");
}

void TestToTimecode_MissingSessionSR_FallsBackToBase() {
    // sessionSampleRate == 0 → fall back to basePosition
    std::vector<LivePtslSpotFile> files = { MakeFile(48000, 48000, 96000) };
    const auto pos = ComputeSpotPositions(files, SpotMode::ToTimecode, 777, 0);
    Require(pos.size() == 1, "ToTimecode missing sessionSR: one position");
    Require(pos[0] == 777, "ToTimecode missing sessionSR: falls back to base");
}

// ── Edge cases ────────────────────────────────────────────────────────────────

void TestEmptyFiles() {
    std::vector<LivePtslSpotFile> files;
    Require(ComputeSpotPositions(files, SpotMode::AtCursor, 0, 48000).empty(),
            "empty file list: empty positions");
}

void TestSessionSR_Zero_UsesRaw() {
    // sessionSampleRate == 0 → no conversion, use raw durationSamples
    std::vector<LivePtslSpotFile> files = {
        MakeFile(44100, 44100),
        MakeFile(44100, 44100),
    };
    const auto pos = ComputeSpotPositions(files, SpotMode::AtCursor, 0, 0);
    Require(pos.size() == 2, "sessionSR zero: two positions");
    Require(pos[1] == 44100, "sessionSR zero: raw durationSamples used as offset");
}

} // namespace

int main() {
    TestAtCursor_SingleFile();
    TestAtCursor_MultiFile_SameSR();
    TestAtCursor_MultiFile_SRUpscale();
    TestAtCursor_MultiFile_SRDownscale();
    TestAtCursor_MissingDuration();
    TestToSelectionIn_IsSameAsAtCursor();
    TestToTimecode_SameSR();
    TestToTimecode_SRConvert();
    TestToTimecode_MultiFile_Independent();
    TestToTimecode_MissingBWF_FallsBackToBase();
    TestToTimecode_MissingSessionSR_FallsBackToBase();
    TestEmptyFiles();
    TestSessionSR_Zero_UsesRaw();
    std::cout << "TestPtslOffsetLayout passed\n";
    return 0;
}

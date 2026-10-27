// test_diag_filter.cpp
// Unit tests for DiagnosticsLogger::getRecentFilteredLines()
// Validates: Metric exclusion, consecutive-run collapse, maxLines capping.
// AURA_BUILD_DIAGNOSTICS must be 1 (enforced in CMakeLists.txt).

#include "DiagnosticsLogger.h"
#include <iostream>
#include <string>
#include <cstring>
#include <thread>
#include <chrono>

static int s_failed = 0;

#define ASSERT(cond, msg) do { \
    if (!(cond)) { std::cerr << "FAIL: " << (msg) << "\n"; ++s_failed; } \
    else          { std::cout << "PASS: " << (msg) << "\n"; } \
} while(0)

// Wait for the background writer thread to drain the queue into the ring.
// The writer sleeps 500ms when idle, so we wait slightly longer.
static void waitDrain() {
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
}

// Count occurrences of a substring in a string.
static int countOccurrences(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return 0;
    int count = 0;
    size_t pos = 0;
    while ((pos = haystack.find(needle, pos)) != std::string::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

int main() {
    auto& log = DiagnosticsLogger::instance();
    log.setEnabled(true);
    log.setMaxEntries(512);

    // ----------------------------------------------------------------
    // Test 1: Metric entries must be excluded
    // ----------------------------------------------------------------
    log.clear();
    log.logMetric("FPS", "frame_rate", 59.9);
    log.logMetric("Mem", "used_mb", 128.0);
    log.logAction("Audio", "play(): started=1");
    log.logMetric("FPS", "frame_rate", 60.0);
    waitDrain();

    std::string out1 = log.getRecentFilteredLines(50);
    ASSERT(out1.find("\"FPS\"") == std::string::npos,
           "T1: Metric category FPS must be absent");
    ASSERT(out1.find("\"Mem\"") == std::string::npos,
           "T1: Metric category Mem must be absent");
    ASSERT(out1.find("\"Audio\"") != std::string::npos,
           "T1: Action category Audio must be present");

    // ----------------------------------------------------------------
    // Test 2: Consecutive same (category, detail) runs are collapsed
    // ----------------------------------------------------------------
    log.clear();
    // 8 identical CoverWall.Summary entries followed by an Audio entry
    for (int i = 0; i < 8; ++i)
        log.logAction("CoverWall", "Summary: total=17 ready=17 cache=17 warmup=0 fail60=0");
    log.logAction("Audio", "play(): started=1");
    waitDrain();

    std::string out2 = log.getRecentFilteredLines(50);
    // The 8 identical CoverWall lines must collapse:
    // tryMergeWithLastLocked() merges within 2s → 1 ring entry with repeatCount>=2
    // OR getRecentFilteredLines() run-collapse → 1 output line with collapsed>1
    // Either way: only 1 "CoverWall" JSON object in output.
    int cwCount = countOccurrences(out2, "\"CoverWall\"");
    ASSERT(cwCount == 1,
           "T2: 8 identical CoverWall entries must collapse to 1 (got " +
           std::to_string(cwCount) + ")");
    // Collapsed by ring merge (repeatCount) or by our post-process (collapsed field)
    bool hasRepeatOrCollapse = (out2.find("\"repeat\":") != std::string::npos &&
                                out2.find("\"repeat\":1,") == std::string::npos) ||
                               out2.find("\"collapsed\":") != std::string::npos;
    ASSERT(hasRepeatOrCollapse,
           "T2: merged CoverWall entry must show repeat>1 or collapsed field");
    // Audio entry still present
    ASSERT(out2.find("\"Audio\"") != std::string::npos,
           "T2: Audio entry must still be present after collapse");

    // ----------------------------------------------------------------
    // Test 3: Non-consecutive identical entries are NOT collapsed
    // ----------------------------------------------------------------
    log.clear();
    log.logAction("CoverWall", "Summary: total=10 ready=5 cache=5 warmup=0 fail60=0");
    log.logAction("Audio",     "play(): started=1");
    log.logAction("CoverWall", "Summary: total=10 ready=5 cache=5 warmup=0 fail60=0");
    waitDrain();

    std::string out3 = log.getRecentFilteredLines(50);
    int cwCount3 = countOccurrences(out3, "\"CoverWall\"");
    ASSERT(cwCount3 == 2,
           "T3: Non-consecutive CoverWall entries must remain separate (got " +
           std::to_string(cwCount3) + ")");
    ASSERT(out3.find("\"collapsed\"") == std::string::npos,
           "T3: No 'collapsed' field when run length is 1");

    // ----------------------------------------------------------------
    // Test 4: maxLines is respected after collapsing
    // ----------------------------------------------------------------
    log.clear();
    // 20 distinct Action entries
    for (int i = 0; i < 20; ++i) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "event_%d", i);
        log.logAction("Test", buf);
    }
    waitDrain();

    std::string out4 = log.getRecentFilteredLines(5);
    int lineCount4 = countOccurrences(out4, "\"category\"");
    ASSERT(lineCount4 == 5,
           "T4: maxLines=5 must yield exactly 5 entries (got " +
           std::to_string(lineCount4) + ")");
    // Should contain the most recent entries (event_15..event_19)
    ASSERT(out4.find("event_19") != std::string::npos,
           "T4: Last event (event_19) must be present when capped to 5");
    ASSERT(out4.find("event_0") == std::string::npos,
           "T4: First event (event_0) must be dropped when capped to 5");

    // ----------------------------------------------------------------
    // Test 5: Performance entries are kept (not filtered)
    // ----------------------------------------------------------------
    log.clear();
    log.logPerformance("Waveform", "ProgressiveMainWaveDecode", 12.5);
    log.logMetric("Mem", "used_mb", 64.0);  // should be filtered
    log.logPerformance("Search",   "FtsQuery", 3.1);
    waitDrain();

    std::string out5 = log.getRecentFilteredLines(50);
    ASSERT(out5.find("\"Waveform\"") != std::string::npos,
           "T5: Performance/Waveform entry must be present");
    ASSERT(out5.find("\"Search\"") != std::string::npos,
           "T5: Performance/Search entry must be present");
    ASSERT(out5.find("\"Mem\"") == std::string::npos,
           "T5: Metric/Mem entry must be absent");

    // ----------------------------------------------------------------
    // Result
    // ----------------------------------------------------------------
    if (s_failed == 0) {
        std::cout << "\nAll diag filter tests PASSED.\n";
        return 0;
    } else {
        std::cerr << "\n" << s_failed << " test(s) FAILED.\n";
        return 1;
    }
}

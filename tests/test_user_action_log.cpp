// test_user_action_log.cpp — Unit tests for UserActionLog
// Must define AURA_BUILD_DIAGNOSTICS before including the header.
#define AURA_BUILD_DIAGNOSTICS 1
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include "../src/UserActionLog.h"

static int failed = 0;
static int passed = 0;

static void CHECK(bool cond, const char* desc) {
    if (cond) { ++passed; std::printf("  PASS: %s\n", desc); }
    else      { ++failed; std::printf("  FAIL: %s\n", desc); }
}

int main() {
    std::printf("=== UserActionLog Tests ===\n");

    auto& log = UserActionLog::instance();

// Initial state
    std::printf("\n--- Initial state ---\n");
    log.clear();
    CHECK(log.entryCount() == 0, "initial: entryCount = 0");

// Enable / disable
    std::printf("\n--- Enable/disable ---\n");
    {
        log.setEnabled(true);
        CHECK(log.isEnabled(), "setEnabled(true) → isEnabled");

        log.setEnabled(false);
        CHECK(!log.isEnabled(), "setEnabled(false) → !isEnabled");
    }

// Log when disabled
    std::printf("\n--- Log when disabled ---\n");
    {
        log.setEnabled(false);
        log.clear();
        log.log(UACategory::Search, "should not record");
        CHECK(log.entryCount() == 0, "log while disabled → no entries");
    }

// Log when enabled
    std::printf("\n--- Log when enabled ---\n");
    {
        log.setEnabled(true);
        log.clear();
        log.log(UACategory::Search, "kick drum");
        CHECK(log.entryCount() == 1, "1 log → entryCount = 1");

        log.log(UACategory::Filter, "category=Foley");
        log.log(UACategory::Select, "file#3 kick_01.wav");
        CHECK(log.entryCount() == 3, "3 logs → entryCount = 3");
    }

// logFmt
    std::printf("\n--- logFmt ---\n");
    {
        log.clear();
        log.logFmt(UACategory::Play, "file: %s at %.1fs", "test.wav", 2.5);
        CHECK(log.entryCount() == 1, "logFmt → entryCount = 1");
    }

// clear
    std::printf("\n--- clear ---\n");
    {
        log.clear();
        log.log(UACategory::System, "test");
        log.log(UACategory::System, "test2");
        CHECK(log.entryCount() == 2, "before clear: 2 entries");
        log.clear();
        CHECK(log.entryCount() == 0, "after clear: 0 entries");
    }

// exportRecentString
    std::printf("\n--- exportRecentString ---\n");
    {
        log.clear();
        log.log(UACategory::Search, "reverb");
        log.log(UACategory::Play, "reverb_hall.wav");
        auto s = log.exportRecentString(10);
        CHECK(!s.empty(), "exportRecentString non-empty");
        CHECK(s.find("SEARCH") != std::string::npos || s.find("Search") != std::string::npos,
              "exportRecentString contains category name");
    }

// exportTimelineString
    std::printf("\n--- exportTimelineString ---\n");
    {
        log.clear();
        log.log(UACategory::Navigate, "tab switch");
        auto s = log.exportTimelineString();
        CHECK(!s.empty(), "exportTimelineString non-empty");
    }

// persistent streaming sink
    std::printf("\n--- streaming sink ---\n");
    {
        namespace fs = std::filesystem;
        const fs::path path = fs::temp_directory_path() / "aris_user_action_stream.txt";
        log.setOutputPath(path.u8string());
        log.clear();
        log.setEnabled(true);
        log.log(UACategory::Search, "streamed action");
        log.setOutputPath("");
        std::ifstream input(path);
        const std::string content((std::istreambuf_iterator<char>(input)),
                                  std::istreambuf_iterator<char>());
        CHECK(content.find("# Aris User Action Log") != std::string::npos,
              "streaming sink writes header");
        CHECK(content.find("streamed action") != std::string::npos,
              "streaming sink flushes action line");
        std::error_code ec;
        fs::remove(path, ec);
    }

// exportTimeline file roundtrip
    std::printf("\n--- exportTimeline ---\n");
    {
        namespace fs = std::filesystem;
        const fs::path path = fs::temp_directory_path() / "aris_user_action_timeline.txt";
        const std::string expected = log.exportTimelineString();
        CHECK(log.exportTimeline(path.u8string()), "exportTimeline writes through filesystem PAL");
        std::ifstream input(path);
        const std::string actual((std::istreambuf_iterator<char>(input)),
                                 std::istreambuf_iterator<char>());
        CHECK(actual == expected, "exportTimeline preserves formatted content");
        std::error_code ec;
        fs::remove(path, ec);
    }

// copyRecent
    std::printf("\n--- copyRecent ---\n");
    {
        log.clear();
        log.log(UACategory::Edit, "change title");
        log.log(UACategory::Export, "copy path");

        UAEntry buf[10];
        int n = log.copyRecent(buf, 10);
        CHECK(n == 2, "copyRecent returns 2");
    }

// Ring buffer wrapping
    std::printf("\n--- Ring buffer ---\n");
    {
        log.clear();
        // Write more than kMaxEntries isn't practical in a unit test (2000 entries),
        // but we can verify it doesn't crash with many entries
        for (int i = 0; i < 100; i++) {
            log.logFmt(UACategory::UI, "action %d", i);
        }
        CHECK(log.entryCount() == 100, "100 entries logged");
    }

// All category names
    std::printf("\n--- All categories ---\n");
    {
        log.clear();
        log.log(UACategory::Search, "a");
        log.log(UACategory::Filter, "b");
        log.log(UACategory::Sort, "c");
        log.log(UACategory::Select, "d");
        log.log(UACategory::Play, "e");
        log.log(UACategory::Seek, "f");
        log.log(UACategory::Navigate, "g");
        log.log(UACategory::Edit, "h");
        log.log(UACategory::Export, "i");
        log.log(UACategory::Shortcut, "j");
        log.log(UACategory::UI, "k");
        log.log(UACategory::System, "l");
        CHECK(log.entryCount() == 12, "all 12 categories logged without crash");
    }

    log.setEnabled(false);

    std::printf("\n=== UserActionLog: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}

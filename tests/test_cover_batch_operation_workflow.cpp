#include "src/CoverBatchOperationWorkflow.h"

#include <cstdint>
#include <iostream>
#include <regex>
#include <string>
#include <vector>

using namespace aura::cover_batch;

namespace {
int failures = 0;

void Check(bool value, const char* message) {
    if (value) return;
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
}

struct FakePersistence final : CoverBatchPersistencePort {
    bool thumbExists = false;
    std::int64_t now = 123;
    std::vector<std::string> trace;

    bool hasCoverThumb(const std::string& key) override {
        trace.push_back("has:" + key);
        return thumbExists;
    }
    void saveCoverThumb(const std::string& key,
                        const std::vector<std::uint8_t>& data,
                        const std::string& mime) override {
        trace.push_back("save:" + key + ":" + std::to_string(data.size()) + ":" + mime);
    }
    void assignEmbeddedCoverByPaths(const std::vector<std::string>& paths,
                                    const std::string& key,
                                    std::int64_t timestamp) override {
        trace.push_back("assign:" + key + ":" + std::to_string(timestamp) +
                        ":" + std::to_string(paths.size()));
    }
    void clearCoverByPaths(const std::vector<std::string>& paths) override {
        trace.push_back("clear:" + std::to_string(paths.size()));
    }
    std::int64_t currentUnixTime() override {
        trace.push_back("time");
        return now;
    }
};

struct FakeView final : CoverBatchViewPort {
    std::vector<std::string> trace;

    void applyVisibleRows(const std::vector<std::string>& paths,
                          bool paste,
                          const std::string& key) override {
        trace.push_back(std::string("rows:") + (paste ? "paste:" : "delete:") +
                        key + ":" + std::to_string(paths.size()));
    }
    void invalidateFileCoverCaches(const std::vector<std::string>& paths) override {
        trace.push_back("invalidate:" + std::to_string(paths.size()));
    }
    void clearFolderCoverNegativeCache() override { trace.push_back("negative"); }
    void requestCoverWallRefresh() override { trace.push_back("wall"); }
    void syncToTab() override { trace.push_back("sync"); }
};

struct FakeState final : CoverBatchStatePort {
    std::vector<std::string> trace;
    void finishBatch(int total) override {
        trace.push_back("finish:" + std::to_string(total));
    }
};

struct RunResult {
    bool consumed = false;
    FakePersistence persistence;
    FakeView view;
    FakeState state;
};

RunResult Run(const CoverBatchOperationInput& input, bool thumbExists = false) {
    RunResult result;
    result.persistence.thumbExists = thumbExists;
    result.consumed = PumpCoverBatchOperation(
        input, result.persistence, result.view, result.state);
    return result;
}
}  // namespace

int main() {
    const std::vector<std::string> targets{"a.wav", "b.wav"};
    const std::vector<std::uint8_t> png{0x89, 'P', 'N', 'G', 1, 2, 3, 4};
    const std::string emptyMime;

    {
        auto result = Run({false, true, 0, 2, targets, png, emptyMime});
        Check(!result.consumed, "inactive batch is rejected");
        Check(result.persistence.trace.empty() && result.view.trace.empty() &&
                  result.state.trace.empty(),
              "inactive batch has no effects");
    }
    {
        auto result = Run({true, true, 1, 2, targets, png, emptyMime});
        Check(result.consumed, "active cleanup-only batch is consumed");
        Check(result.persistence.trace.empty() && result.view.trace.empty(),
              "cleanup-only batch skips transaction effects");
        Check(result.state.trace == std::vector<std::string>{"finish:2"},
              "cleanup-only batch still terminates");
    }
    {
        auto result = Run({true, true, 0, 2, targets, png, emptyMime});
        Check(result.persistence.trace.size() == 4,
              "paste miss performs time, lookup, save, and assign");
        Check(result.persistence.trace[0] == "time", "paste reads time first");
        const std::string key = result.persistence.trace[1].substr(4);
        Check(std::regex_match(key, std::regex("E\\|[0-9a-f]{16}\\|8\\|image/png")),
              "paste key preserves exact format and PNG MIME");
        Check(result.persistence.trace[2] == "save:" + key + ":8:image/png",
              "thumbnail miss persists clipboard bytes");
        Check(result.persistence.trace[3] == "assign:" + key + ":123:2",
              "paste assigns the same key and timestamp");
        Check(result.view.trace == std::vector<std::string>({
                  "rows:paste:" + key + ":2", "invalidate:2", "wall", "sync"}),
              "paste preserves visible/cache refresh order");
        Check(result.state.trace == std::vector<std::string>{"finish:2"},
              "paste terminates after view effects");
    }
    {
        auto result = Run({true, true, 0, 2, targets, png, emptyMime}, true);
        Check(result.persistence.trace.size() == 3,
              "thumbnail hit skips duplicate save");
        Check(result.persistence.trace[1].rfind("has:", 0) == 0 &&
                  result.persistence.trace[2].rfind("assign:", 0) == 0,
              "thumbnail hit still assigns the key");
    }
    {
        std::vector<std::uint8_t> atLimit(512 * 1024, 7);
        std::vector<std::uint8_t> overLimit(512 * 1024 + 1, 7);
        auto at = Run({true, true, 0, 1, targets, atLimit, emptyMime});
        auto over = Run({true, true, 0, 1, targets, overLimit, emptyMime});
        Check(at.persistence.trace.size() == 4,
              "512 KiB thumbnail remains eligible for persistence");
        Check(over.persistence.trace.size() == 3,
              "thumbnail above 512 KiB is not persisted");
    }
    {
        auto result = Run({true, false, 0, 2, targets, png, emptyMime});
        Check(result.persistence.trace == std::vector<std::string>{"time", "clear:2"},
              "delete preserves time read then database clear");
        Check(result.view.trace == std::vector<std::string>({
                  "rows:delete::2", "invalidate:2", "negative", "wall", "sync"}),
              "delete preserves row/cache refresh order");
        Check(result.state.trace == std::vector<std::string>{"finish:2"},
              "delete terminates last");
    }

    const std::vector<std::uint8_t> jpeg{0xFF, 0xD8, 0xFF};
    const std::vector<std::uint8_t> unknown{1, 2, 3};
    Check(ResolveCoverImageMime(jpeg, emptyMime) == "image/jpeg",
          "JPEG signature is detected");
    Check(ResolveCoverImageMime(png, emptyMime) == "image/png",
          "PNG signature is detected");
    Check(ResolveCoverImageMime(unknown, emptyMime) == "image/jpeg",
          "unknown image falls back to JPEG");
    Check(ResolveCoverImageMime(unknown, "image/webp") == "image/webp",
          "explicit MIME hint wins");

    if (failures) return 1;
    std::cout << "PASS: cover batch operation workflow\n";
    return 0;
}

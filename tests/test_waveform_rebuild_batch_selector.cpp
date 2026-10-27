#include "src/WaveformRebuildBatchSelector.h"

#include <cstdio>
#include <string>
#include <utility>
#include <vector>

std::vector<AudioRecord> Database::getAllFiles(int, int) {
    return {};
}

WaveformCachePolicy::Mode Database::getWaveformCacheMode() {
    return WaveformCachePolicy::Mode::MiniOnly;
}

std::vector<int64_t> Database::getFilesNeedingWaveformCache(int) {
    return {};
}

std::vector<int64_t> Database::getFilesNeedingDetailCache(int) {
    return {};
}

std::vector<AudioRecord> Database::getFilesByIds(const std::vector<int64_t>&) {
    return {};
}

namespace {

int g_failures = 0;

void Check(bool condition, const char* message) {
    if (condition) {
        std::printf("PASS: %s\n", message);
    } else {
        std::printf("FAIL: %s\n", message);
        ++g_failures;
    }
}

AudioRecord Record(int64_t id, const char* path) {
    AudioRecord record;
    record.id = id;
    record.filePath = path;
    return record;
}

class FakeSource final : public aura::waveform_rebuild::WaveformRebuildBatchSource {
public:
    std::vector<AudioRecord> allRecords;
    std::vector<int64_t> miniIds;
    std::vector<int64_t> detailIds;
    std::vector<AudioRecord> resolvedRecords;
    bool miniOnlyCacheMode = false;
    mutable std::vector<std::string> calls;

    std::vector<AudioRecord> getAllFiles(int limit, int offset) override {
        calls.push_back("all:" + std::to_string(limit) + ":" + std::to_string(offset));
        return allRecords;
    }

    bool usesMiniOnlyCacheMode() const override {
        calls.push_back("mode");
        return miniOnlyCacheMode;
    }

    std::vector<int64_t> getFilesNeedingWaveformCache(int limit) override {
        calls.push_back("mini:" + std::to_string(limit));
        return miniIds;
    }

    std::vector<int64_t> getFilesNeedingDetailCache(int limit) override {
        calls.push_back("detail:" + std::to_string(limit));
        return detailIds;
    }

    std::vector<AudioRecord> getFilesByIds(const std::vector<int64_t>&) override {
        calls.push_back("by_ids");
        return resolvedRecords;
    }
};

}  // namespace

int main() {
    using aura::waveform_rebuild::SelectWaveformRebuildBatch;
    using aura::waveform_rebuild::WaveformRebuildBatchRequest;

    FakeSource full;
    full.allRecords = {Record(1, "full.wav")};
    full.miniOnlyCacheMode = true;
    const auto fullSelection = SelectWaveformRebuildBatch(
        full, WaveformRebuildBatchRequest{true, false, 500, 100});
    Check(fullSelection.miniOnly && fullSelection.records.size() == 1,
          "full selection returns records and follows cache mode");
    Check(full.calls == std::vector<std::string>{"all:500:100", "mode"},
          "full selection preserves offset query order");

    FakeSource mini;
    mini.miniIds = {2};
    mini.resolvedRecords = {Record(2, "mini.wav")};
    const auto miniSelection = SelectWaveformRebuildBatch(
        mini, WaveformRebuildBatchRequest{false, true, 500, 0});
    Check(miniSelection.miniOnly && miniSelection.records.size() == 1,
          "incremental mini selection classifies a non-empty batch as mini-only");
    Check(mini.calls == std::vector<std::string>{"mini:500", "by_ids"},
          "incremental mini selection queries only mini debt");

    FakeSource detail;
    detail.detailIds = {3};
    detail.miniIds = {4};
    detail.resolvedRecords = {Record(3, "detail.wav")};
    const auto detailSelection = SelectWaveformRebuildBatch(
        detail, WaveformRebuildBatchRequest{false, false, 500, 0});
    Check(!detailSelection.miniOnly && detailSelection.records.size() == 1,
          "detail selection remains detail-first");
    Check(detail.calls == std::vector<std::string>{"detail:500", "by_ids"},
          "detail selection does not query mini debt when detail IDs exist");

    FakeSource fallback;
    fallback.miniIds = {5};
    fallback.resolvedRecords = {Record(5, "fallback.wav")};
    const auto fallbackSelection = SelectWaveformRebuildBatch(
        fallback, WaveformRebuildBatchRequest{false, false, 500, 0});
    Check(fallbackSelection.miniOnly && fallbackSelection.records.size() == 1,
          "empty detail selection falls back to a mini-only batch");
    Check(fallback.calls ==
              std::vector<std::string>{"detail:500", "mini:500", "by_ids"},
          "empty detail selection falls back to mini debt once");

    FakeSource empty;
    const auto emptySelection = SelectWaveformRebuildBatch(
        empty, WaveformRebuildBatchRequest{false, false, 500, 0});
    Check(!emptySelection.miniOnly && emptySelection.records.empty(),
          "empty detail and mini debt returns an unclassified empty batch");
    Check(empty.calls ==
              std::vector<std::string>{"detail:500", "mini:500", "by_ids"},
          "empty selection preserves the legacy final ID resolution call");

    std::printf("\n=== Waveform Rebuild Batch Selector: %d failure(s) ===\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}

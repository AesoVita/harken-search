#include "PlaybackHistoryStateAdapter.h"

#include <cstdint>
#include <iostream>

namespace {

struct CountingSink {
    int increments = 0;
    int updates = 0;
    std::int64_t lastIncrementId = 0;
    std::int64_t lastUpdateId = 0;

    bool incrementPlayCount(std::int64_t fileId) {
        ++increments;
        lastIncrementId = fileId;
        return true;
    }

    bool updateLastPlayed(std::int64_t fileId) {
        ++updates;
        lastUpdateId = fileId;
        return true;
    }
};

bool Check(bool condition, const char* message) {
    if (condition) return true;
    std::cerr << "FAIL: " << message << '\n';
    return false;
}

}  // namespace

int main() {
    using aura::playback::PlayStatsEvent;
    using aura::playback::PlayStatsGate;
    using aura::playback::RecordSuccessfulPlayStats;

    PlayStatsGate gate;
    CountingSink sink;

    const PlayStatsEvent clickIntent{
        false, false, 11, 1};
    if (!Check(!RecordSuccessfulPlayStats(sink, gate, clickIntent),
               "click intent must not record play stats"))
        return 1;
    if (!Check(sink.increments == 0 && sink.updates == 0,
               "click intent must not touch play_count/last_played"))
        return 1;

    const PlayStatsEvent failedLoad{false, false, 11, 2};
    if (!Check(!RecordSuccessfulPlayStats(sink, gate, failedLoad),
               "failed load must not record play stats"))
        return 1;

    const PlayStatsEvent staleOrCancel{false, true, 11, 3};
    if (!Check(!RecordSuccessfulPlayStats(sink, gate, staleOrCancel),
               "stale/cancel pump must not record play stats"))
        return 1;

    const PlayStatsEvent unloadedFile{true, true, 0, 4};
    if (!Check(!RecordSuccessfulPlayStats(sink, gate, unloadedFile),
               "fileId<=0 must not record play stats"))
        return 1;

    const PlayStatsEvent loadedWithoutPlay{true, false, 11, 5};
    if (!Check(!RecordSuccessfulPlayStats(sink, gate, loadedWithoutPlay),
               "successful load without start of playback must not record"))
        return 1;

    const PlayStatsEvent success{true, true, 11, 6};
    if (!Check(RecordSuccessfulPlayStats(sink, gate, success),
               "successful start of playback must record once"))
        return 1;
    if (!Check(sink.increments == 1 && sink.updates == 1,
               "successful start must call incrementPlayCount and updateLastPlayed once"))
        return 1;
    if (!Check(sink.lastIncrementId == 11 && sink.lastUpdateId == 11,
               "successful start must record the loaded file id"))
        return 1;

    if (!Check(!RecordSuccessfulPlayStats(sink, gate, success),
               "the same successful load token must not double-count"))
        return 1;
    if (!Check(sink.increments == 1 && sink.updates == 1,
               "double consume of the same load must leave counters unchanged"))
        return 1;

    const PlayStatsEvent nextSuccess{true, true, 12, 7};
    if (!Check(RecordSuccessfulPlayStats(sink, gate, nextSuccess),
               "a later successful load must record again"))
        return 1;
    if (!Check(sink.increments == 2 && sink.updates == 2 &&
                   sink.lastIncrementId == 12,
               "next successful load must increment a different file"))
        return 1;

    std::cout << "PASS: play stats recorder\n";
    return 0;
}

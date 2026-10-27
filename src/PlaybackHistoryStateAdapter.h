#pragma once

#include <cstdint>
#include <string>

struct AppState;

namespace aura::playback {

void MarkFilePlayed(
    AppState& state,
    const std::string& filePath,
    std::int64_t fileId);

void RecordFilePlayed(
    AppState& state,
    const std::string& filePath,
    std::int64_t fileId,
    bool addToHistory);

bool IsFilePlayed(const AppState& state, const std::string& filePath);

struct PlayStatsEvent {
    bool loadSucceeded = false;
    bool playbackStarted = false;
    std::int64_t fileId = 0;
    std::uint64_t loadToken = 0;
};

struct PlayStatsGate {
    bool hasRecorded = false;
    std::uint64_t lastRecordedToken = 0;
};

template <typename PlayStatsSink>
bool RecordSuccessfulPlayStats(
    PlayStatsSink& sink,
    PlayStatsGate& gate,
    const PlayStatsEvent& event) {
    if (!event.loadSucceeded || !event.playbackStarted || event.fileId <= 0) {
        return false;
    }
    if (gate.hasRecorded && gate.lastRecordedToken == event.loadToken) {
        return false;
    }
    if (!sink.incrementPlayCount(event.fileId)) {
        return false;
    }
    sink.updateLastPlayed(event.fileId);
    gate.hasRecorded = true;
    gate.lastRecordedToken = event.loadToken;
    return true;
}

}  // namespace aura::playback

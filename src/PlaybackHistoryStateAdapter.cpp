#include "PlaybackHistoryStateAdapter.h"

#include "AppState.h"

namespace aura::playback {

void RecordFilePlayed(
    AppState& state,
    const std::string& filePath,
    std::int64_t fileId,
    bool addToHistory) {
    state.playedFiles.insert(filePath);
    if (fileId > 0) {
        state.playedFileIds.insert(fileId);
    }
    if (!addToHistory) return;

    const std::size_t lastSlash = filePath.find_last_of("/\\");
    const std::string fileName =
        lastSlash == std::string::npos
            ? filePath
            : filePath.substr(lastSlash + 1);
    state.addToPlayHistory(fileId, filePath, fileName);
}

void MarkFilePlayed(
    AppState& state,
    const std::string& filePath,
    std::int64_t fileId) {
    RecordFilePlayed(state, filePath, fileId, true);
}

bool IsFilePlayed(const AppState& state, const std::string& filePath) {
    return state.playedFiles.find(filePath) != state.playedFiles.end();
}

}  // namespace aura::playback

void MarkFilePlayed(AppState& state,
                    const std::string& filePath,
                    int64_t fileId,
                    bool addToHistory) {
    aura::playback::RecordFilePlayed(
        state, filePath, fileId, addToHistory);
}

bool IsFilePlayed(const AppState& state,
                  const std::string& filePath) {
    return aura::playback::IsFilePlayed(state, filePath);
}

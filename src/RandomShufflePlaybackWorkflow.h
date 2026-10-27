#pragma once

#include <cstdint>
#include <string>

namespace aura::search {

struct RandomShufflePlaybackInput {
    bool randomShuffle = false;
    std::string filePath;
    std::int64_t recordId = 0;
    int waveformDetail = 0;
};

class RandomShufflePlaybackEffects {
public:
    virtual ~RandomShufflePlaybackEffects() = default;
    virtual void loadFileAsync(const std::string& filePath, bool autoplay) = 0;
    virtual void scheduleWaveformDetail(const std::string& filePath, int detail) = 0;
    virtual void resetWaveformZoom() = 0;
    virtual void markFilePlayed(const std::string& filePath, std::int64_t recordId) = 0;
    virtual void clearSelection() = 0;
    virtual void selectIndex(int index) = 0;
    virtual void beginAudioTarget(int index) = 0;
    virtual void setLastClickedIndex(int index) = 0;
    virtual void setScrollToSelected(bool enabled) = 0;
    virtual void synchronizeView() = 0;
    virtual void reportTriggered() = 0;
};

bool HandleRandomShufflePlayback(
    const RandomShufflePlaybackInput& input,
    RandomShufflePlaybackEffects& effects);

}  // namespace aura::search

#include "RandomShufflePlaybackWorkflow.h"

namespace aura::search {

bool HandleRandomShufflePlayback(
    const RandomShufflePlaybackInput& input,
    RandomShufflePlaybackEffects& effects) {
    if (!input.randomShuffle || input.filePath.empty()) return false;
    effects.loadFileAsync(input.filePath, true);
    effects.scheduleWaveformDetail(input.filePath, input.waveformDetail);
    effects.resetWaveformZoom();
    effects.markFilePlayed(input.filePath, input.recordId);
    effects.clearSelection();
    effects.selectIndex(0);
    effects.beginAudioTarget(0);
    effects.setLastClickedIndex(0);
    effects.setScrollToSelected(true);
    effects.synchronizeView();
    effects.reportTriggered();
    return true;
}

}  // namespace aura::search

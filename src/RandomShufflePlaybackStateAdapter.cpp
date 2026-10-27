#include "RandomShufflePlaybackStateAdapter.h"

#include "AppState.h"
#include "AsyncWorker.h"
#include "DiagnosticsLogger.h"

#include <utility>

namespace aura::search {

namespace {

class Effects final : public RandomShufflePlaybackEffects {
public:
    Effects(
        AppState& state,
        AsyncSearchRandomShufflePlaybackStateAdapter::ScheduleWaveformDetail
            scheduleWaveformDetail,
        AsyncSearchRandomShufflePlaybackStateAdapter::ResetWaveformZoom
            resetWaveformZoom,
        AsyncSearchRandomShufflePlaybackStateAdapter::MarkFilePlayed
            markFilePlayed)
        : state_(state),
          scheduleWaveformDetail_(std::move(scheduleWaveformDetail)),
          resetWaveformZoom_(std::move(resetWaveformZoom)),
          markFilePlayed_(std::move(markFilePlayed)) {}

    void loadFileAsync(const std::string& filePath, bool autoplay) override {
        state_.audioEngine.loadFileAsync(filePath, autoplay);
    }
    void scheduleWaveformDetail(
        const std::string& filePath,
        int detail) override {
        scheduleWaveformDetail_(state_, filePath, detail);
    }
    void resetWaveformZoom() override { resetWaveformZoom_(state_); }
    void markFilePlayed(
        const std::string& filePath,
        std::int64_t recordId) override {
        markFilePlayed_(state_, filePath, recordId);
    }
    void clearSelection() override { state_.selectedIndicesSet.clear(); }
    void selectIndex(int index) override {
        state_.selectedIndicesSet.insert(index);
    }
    void beginAudioTarget(int index) override {
        state_.beginAudioTargetFromSearchResult(index);
    }
    void setLastClickedIndex(int index) override {
        state_.lastClickedIndex = index;
    }
    void setScrollToSelected(bool enabled) override {
        state_.scrollToSelected = enabled;
    }
    void synchronizeView() override { state_.syncToTab(); }
    void reportTriggered() override {
        DIAG_ACTION("Search", "Async Random Playback Triggered (1M+ rows)");
    }

private:
    AppState& state_;
    AsyncSearchRandomShufflePlaybackStateAdapter::ScheduleWaveformDetail
        scheduleWaveformDetail_;
    AsyncSearchRandomShufflePlaybackStateAdapter::ResetWaveformZoom
        resetWaveformZoom_;
    AsyncSearchRandomShufflePlaybackStateAdapter::MarkFilePlayed
        markFilePlayed_;
};

}  // namespace

AsyncSearchRandomShufflePlaybackStateAdapter::
    AsyncSearchRandomShufflePlaybackStateAdapter(
        AppState& state,
        AsyncSearchResult& result,
        ScheduleWaveformDetail scheduleWaveformDetail,
        ResetWaveformZoom resetWaveformZoom,
        MarkFilePlayed markFilePlayed)
    : state_(state),
      result_(result),
      scheduleWaveformDetail_(std::move(scheduleWaveformDetail)),
      resetWaveformZoom_(std::move(resetWaveformZoom)),
      markFilePlayed_(std::move(markFilePlayed)) {}

void AsyncSearchRandomShufflePlaybackStateAdapter::apply() {
    const auto* first = state_.searchResults.empty()
        ? nullptr
        : &state_.searchResults.front();
    Effects effects(
        state_,
        scheduleWaveformDetail_,
        resetWaveformZoom_,
        markFilePlayed_);
    (void)HandleRandomShufflePlayback(
        {result_.isRandomShuffle,
         first ? first->filePath : std::string{},
         first ? first->id : 0,
         AppState::WAVEFORM_DEFAULT_DETAIL_PEAKS},
        effects);
}

}  // namespace aura::search

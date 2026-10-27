#include "RandomShufflePlaybackWorkflow.h"

#include <cassert>
#include <string>
#include <vector>

namespace {
struct Effects final : aura::search::RandomShufflePlaybackEffects {
    std::vector<std::string> events;
    void loadFileAsync(const std::string&, bool) override { events.push_back("load"); }
    void scheduleWaveformDetail(const std::string&, int) override { events.push_back("waveform"); }
    void resetWaveformZoom() override { events.push_back("zoom"); }
    void markFilePlayed(const std::string&, std::int64_t) override { events.push_back("played"); }
    void clearSelection() override { events.push_back("clear"); }
    void selectIndex(int) override { events.push_back("select"); }
    void beginAudioTarget(int) override { events.push_back("target"); }
    void setLastClickedIndex(int) override { events.push_back("clicked"); }
    void setScrollToSelected(bool) override { events.push_back("scroll"); }
    void synchronizeView() override { events.push_back("sync"); }
    void reportTriggered() override { events.push_back("diag"); }
};
}

int main() {
    Effects effects;
    const aura::search::RandomShufflePlaybackInput input{true, "song.wav", 42, 2048};
    assert(aura::search::HandleRandomShufflePlayback(input, effects));
    assert((effects.events == std::vector<std::string>{
        "load", "waveform", "zoom", "played", "clear", "select",
        "target", "clicked", "scroll", "sync", "diag"}));
    Effects inactive;
    assert(!aura::search::HandleRandomShufflePlayback(
        aura::search::RandomShufflePlaybackInput{false, "song.wav", 42, 2048}, inactive));
    assert(inactive.events.empty());
    return 0;
}

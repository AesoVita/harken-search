#include "frontend/AuditionNativeIntentWorkflow.h"

#include <cassert>
#include <cmath>
#include <string>
#include <vector>

namespace {

using aris::frontend::AuditionNativeIntentWorkflow;
using aris::frontend::AuditionNativeTransportEffects;
using aris::frontend::AuditionPitchMode;

class FakeEffects final : public AuditionNativeTransportEffects {
public:
    bool playing = false;
    bool loaded = true;
    bool reversed = false;
    bool failOnEnable = false;
    bool failOnDisable = false;
    bool loop = false;
    float inPoint = 0.0f;
    float outPoint = 1.0f;
    float fadeIn = 0.0f;
    float fadeOut = 0.0f;
    float position = 0.0f;
    float frequencyShift = 0.0f;
    AuditionPitchMode mode = AuditionPitchMode::Semitone;
    std::vector<std::string> events;

    bool isPlaying() const override { return playing; }
    bool isLoaded() const override { return loaded; }
    bool isReversed() const override { return reversed; }
    bool regionLoopEnabled() const override { return loop; }
    float regionIn() const override { return inPoint; }
    float regionOut() const override { return outPoint; }
    float regionFadeIn() const override { return fadeIn; }
    float regionFadeOut() const override { return fadeOut; }
    float playbackPosition() const override { return position; }
    float currentFrequencyShift() const override { return frequencyShift; }
    AuditionPitchMode pitchMode() const override { return mode; }

    void pause() override { events.emplace_back("pause"); }
    void play() override {
        events.emplace_back("play");
        playing = true;
    }
    void playRegion(bool enabled, float inFade, float outFade) override {
        events.emplace_back(
            std::string("play-region:") + (enabled ? "1" : "0") + ":" +
            std::to_string(inFade) + ":" + std::to_string(outFade));
        playing = true;
    }
    void setPitch(float value) override {
        events.emplace_back("pitch:" + std::to_string(value));
    }
    void setFrequencyShift(float value) override {
        frequencyShift = value;
        events.emplace_back("frequency:" + std::to_string(value));
    }
    void rememberFrequencyShift(float value) override {
        events.emplace_back("remember-frequency:" + std::to_string(value));
    }
    void setConfiguredPitchMode(AuditionPitchMode value) override {
        mode = value;
        events.emplace_back(value == AuditionPitchMode::Semitone
            ? "configure-mode:semitone"
            : "configure-mode:frequency");
    }
    void setEnginePitchMode(AuditionPitchMode value) override {
        events.emplace_back(value == AuditionPitchMode::Semitone
            ? "engine-mode:semitone"
            : "engine-mode:frequency");
    }
    bool setReverse(bool value) override {
        events.emplace_back(std::string("reverse:") + (value ? "1" : "0"));
        const bool willFail = (value ? failOnEnable : failOnDisable);
        if (willFail) {
            reversed = false;
            return false;
        }
        reversed = value;
        return loaded;
    }
    void clearRegion() override { events.emplace_back("clear-region"); }
    void setRegionIn(float value) override {
        inPoint = value;
        events.emplace_back("region-in:" + std::to_string(value));
    }
    void setRegionOut(float value) override {
        outPoint = value;
        events.emplace_back("region-out:" + std::to_string(value));
    }
    void seek(float value) override {
        position = value;
        events.emplace_back("seek:" + std::to_string(value));
    }
    void setRegionLoopEnabled(bool value) override {
        loop = value;
        events.emplace_back(std::string("loop:") + (value ? "1" : "0"));
    }
    void stopLoopKeepCurrentPass() override {
        events.emplace_back("stop-loop-keep-current-pass");
    }
};

}  // namespace

int main() {
    {
        FakeEffects effects;
        effects.playing = true;
        AuditionNativeIntentWorkflow::toggleVisibleFooterPlayback(effects);
        assert((effects.events == std::vector<std::string>{"pause"}));
    }

    {
        FakeEffects effects;
        effects.inPoint = 0.1f;
        effects.outPoint = 0.7f;
        effects.fadeIn = 0.05f;
        effects.loop = true;
        AuditionNativeIntentWorkflow::toggleVisibleFooterPlayback(effects);
        assert(effects.events.size() == 1);
        assert(effects.events.front().rfind("play-region:1:0.050000", 0) == 0);
    }

    {
        FakeEffects effects;
        effects.inPoint = 0.1f;
        effects.outPoint = 0.7f;
        AuditionNativeIntentWorkflow::toggleVisibleFooterPlayback(effects);
        assert((effects.events == std::vector<std::string>{"play"}));
    }

    {
        FakeEffects effects;
        effects.loop = true;
        effects.inPoint = 0.0f;
        effects.outPoint = 1.0f;
        AuditionNativeIntentWorkflow::toggleVisibleFooterPlayback(effects);
        assert(effects.events.size() == 1);
        assert(effects.events.front().rfind("play-region:1:", 0) == 0);
    }

    {
        FakeEffects effects;
        effects.loop = true;
        effects.inPoint = 0.2f;
        effects.outPoint = 0.8f;
        AuditionNativeIntentWorkflow::toggleVisibleFooterPlayback(effects);
        assert(effects.events.size() == 1);
        assert(effects.events.front().rfind("play-region:1:", 0) == 0);
    }

    {
        FakeEffects effects;
        effects.inPoint = 0.1f;
        effects.outPoint = 0.6f;
        AuditionNativeIntentWorkflow::toggleReversePreservingRegion(effects);
        assert((effects.events == std::vector<std::string>{
            "reverse:1",
            "clear-region",
            "region-in:0.400000",
            "region-out:0.900000",
            "seek:0.400000",
            "play",
        }));
    }

    {
        FakeEffects effects;
        effects.position = 0.25f;
        AuditionNativeIntentWorkflow::toggleReversePreservingRegion(effects);
        assert((effects.events == std::vector<std::string>{
            "reverse:1", "seek:0.750000", "play"}));
    }

    {
        FakeEffects effects;
        effects.loaded = false;
        AuditionNativeIntentWorkflow::toggleReversePreservingRegion(effects);
       assert((effects.events == std::vector<std::string>{"reverse:1"}));
    }

    {
        FakeEffects effects;
        effects.failOnEnable = true;
        effects.inPoint = 0.1f;
        effects.outPoint = 0.6f;
        effects.position = 0.2f;
        AuditionNativeIntentWorkflow::toggleReversePreservingRegion(effects);
        assert((effects.events == std::vector<std::string>{"reverse:1"}));
        assert(!effects.reversed);
        assert(effects.inPoint == 0.1f);
        assert(effects.outPoint == 0.6f);
        assert(effects.position == 0.2f);
    }

    {
        FakeEffects effects;
        effects.reversed = true;
        effects.loaded = true;
        effects.failOnDisable = true;
        effects.inPoint = 0.1f;
        effects.outPoint = 0.6f;
        effects.position = 0.2f;
        AuditionNativeIntentWorkflow::toggleReversePreservingRegion(effects);
        assert((effects.events == std::vector<std::string>{"reverse:0"}));
        assert(!effects.reversed);
        assert(effects.inPoint == 0.1f);
        assert(effects.outPoint == 0.6f);
        assert(effects.position == 0.2f);
    }

    {
        FakeEffects effects;
        effects.inPoint = 0.2f;
        effects.outPoint = 0.8f;
        effects.fadeIn = 0.1f;
        effects.fadeOut = 0.2f;
        AuditionNativeIntentWorkflow::setLoopAndRearmRegion(effects, true);
        assert(effects.events.size() == 2);
        assert(effects.events[0] == "loop:1");
        assert(effects.events[1].rfind("play-region:1:0.100000:0.200000", 0) == 0);
    }

    {
        FakeEffects effects;
        effects.inPoint = 0.2f;
        effects.outPoint = 0.8f;
        AuditionNativeIntentWorkflow::setLoopAndRearmRegion(effects, false);
        // Boxed loop-off: re-arm the same region WITHOUT looping; the current
        // pass plays to the region end and must not wrap.
        assert(effects.events.size() == 2);
        assert(effects.events[0] == "loop:0");
        assert(effects.events[1].rfind("play-region:0:", 0) == 0);
    }

    {
        FakeEffects effects;
        effects.inPoint = 0.0f;
        effects.outPoint = 1.0f;
        AuditionNativeIntentWorkflow::setLoopAndRearmRegion(effects, false);
        // Unselected loop-off: disarm in place — never pause, stop or seek.
        assert((effects.events == std::vector<std::string>{
            "loop:0", "stop-loop-keep-current-pass"}));
    }

    {
        FakeEffects effects;
        effects.loaded = false;
        effects.inPoint = 0.0f;
        effects.outPoint = 1.0f;
        AuditionNativeIntentWorkflow::setLoopAndRearmRegion(effects, false);
        assert((effects.events == std::vector<std::string>{"loop:0"}));
    }

    {
        FakeEffects effects;
        effects.inPoint = 0.0f;
        effects.outPoint = 1.0f;
        effects.fadeIn = 0.0f;
        effects.fadeOut = 0.0f;
        AuditionNativeIntentWorkflow::setLoopAndRearmRegion(effects, true);
        assert((effects.events == std::vector<std::string>{
            "loop:1",
            "region-in:0.000000",
            "region-out:1.000000",
            "play-region:1:0.000000:0.000000",
        }));
        assert(effects.inPoint == 0.0f);
        assert(effects.outPoint == 1.0f);
        assert(effects.loop);
        assert(effects.playing);
    }

    {
        FakeEffects effects;
        effects.inPoint = 0.25f;
        effects.outPoint = 0.75f;
        effects.fadeIn = 0.1f;
        effects.fadeOut = 0.2f;
        AuditionNativeIntentWorkflow::setLoopAndRearmRegion(effects, true);
        assert(effects.events.size() == 2);
        assert(effects.events[0] == "loop:1");
        assert(effects.events[1].rfind("play-region:1:0.100000:0.200000", 0) == 0);
        assert(effects.inPoint == 0.25f);
        assert(effects.outPoint == 0.75f);
    }

    {
        FakeEffects effects;
        effects.inPoint = 0.0f;
        effects.outPoint = 1.0f;
        AuditionNativeIntentWorkflow::setLoopAndRearmRegion(effects, true);
        effects.events.clear();
        effects.playing = false;
        AuditionNativeIntentWorkflow::setLoopAndRearmRegion(effects, true);
        assert((effects.events == std::vector<std::string>{
            "loop:1",
            "region-in:0.000000",
            "region-out:1.000000",
            "play-region:1:0.000000:0.000000",
        }));
        assert(effects.inPoint == 0.0f);
        assert(effects.outPoint == 1.0f);
        effects.events.clear();
        effects.playing = false;
        AuditionNativeIntentWorkflow::setLoopAndRearmRegion(effects, true);
        assert((effects.events == std::vector<std::string>{
            "loop:1",
            "region-in:0.000000",
            "region-out:1.000000",
            "play-region:1:0.000000:0.000000",
        }));
        assert(effects.inPoint == 0.0f);
        assert(effects.outPoint == 1.0f);
    }

    {
        FakeEffects effects;
        effects.loaded = false;
        effects.inPoint = 0.0f;
        effects.outPoint = 1.0f;
        AuditionNativeIntentWorkflow::setLoopAndRearmRegion(effects, true);
        assert((effects.events == std::vector<std::string>{"loop:1"}));
        assert(!effects.playing);
    }

    {
        FakeEffects effects;
        effects.mode = AuditionPitchMode::FrequencyShift;
        AuditionNativeIntentWorkflow::setPitchControlValue(effects, 123.0f);
        assert((effects.events == std::vector<std::string>{
            "frequency:123.000000", "remember-frequency:123.000000"}));
    }

    {
        FakeEffects effects;
        AuditionNativeIntentWorkflow::changePitchModeAndReset(
            effects, AuditionPitchMode::FrequencyShift);
        assert((effects.events == std::vector<std::string>{
            "pitch:0.000000",
            "frequency:0.000000",
            "remember-frequency:0.000000",
            "configure-mode:frequency",
            "engine-mode:frequency",
        }));
    }

    return 0;
}

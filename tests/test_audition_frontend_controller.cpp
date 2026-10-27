#include "frontend/AuditionFrontendController.h"
#include "ui/AuditionTransportView.h"

#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using aris::frontend::AuditionAction;
using aris::frontend::AuditionApplicationPort;
using aris::frontend::AuditionDiagnosticCode;
using aris::frontend::AuditionDispatchStatus;
using aris::frontend::AuditionFrontendController;
using aris::frontend::AuditionPitchMode;
using aris::frontend::AuditionStateSnapshot;
using aris::frontend::AuditionUiDispatcher;
using aris::ui::AuditionTransportView;

class FakeAuditionPort final : public AuditionApplicationPort {
public:
    AuditionStateSnapshot snapshot{};
    std::vector<std::string> events;
    float lastValue = 0.0f;
    bool lastEnabled = false;
    AuditionPitchMode lastMode = AuditionPitchMode::Semitone;
    bool throwOnIntent = false;

    void captureSnapshot(AuditionStateSnapshot& output) const override {
        output = snapshot;
    }

    void toggleVisibleFooterPlayback() override {
        beforeIntent("footer-playback");
    }

    void setVolume(float value) override {
        beforeIntent("volume");
        lastValue = value;
    }

    void setPitchControlValue(float value) override {
        beforeIntent("pitch-value");
        lastValue = value;
    }

    void changePitchModeAndReset(AuditionPitchMode mode) override {
        beforeIntent("pitch-mode-reset");
        lastMode = mode;
    }

    void toggleReversePreservingRegion() override {
        beforeIntent("reverse-preserve-region");
    }

    void setLoopAndRearmRegion(bool enabled) override {
        beforeIntent("loop-rearm-region");
        lastEnabled = enabled;
    }

private:
    void beforeIntent(const char* name) {
        if (throwOnIntent) {
            throw std::runtime_error("scripted adapter failure");
        }
        events.emplace_back(name);
    }
};

AuditionStateSnapshot availableSnapshot() {
    AuditionStateSnapshot snapshot{};
    snapshot.audioLoaded = true;
    snapshot.footerPlaybackEnabled = true;
    snapshot.volumeEnabled = true;
    snapshot.pitchEnabled = true;
    snapshot.reverseEnabled = true;
    snapshot.loopEnabled = true;
    snapshot.volumeMinimum = 0.0f;
    snapshot.volumeMaximum = 1.2f;
    snapshot.pitchMinimum = -24.0f;
    snapshot.pitchMaximum = 24.0f;
    snapshot.frequencyShiftMinimum = -5000.0f;
    snapshot.frequencyShiftMaximum = 5000.0f;
    snapshot.pitchMode = AuditionPitchMode::Semitone;
    return snapshot;
}

void assertApplied(const aris::frontend::AuditionDispatchResult& result) {
    assert(result.status == AuditionDispatchStatus::Applied);
    assert(result.diagnostic == AuditionDiagnosticCode::None);
}

}  // namespace

int main() {
    FakeAuditionPort port;
    port.snapshot = availableSnapshot();
    AuditionFrontendController controller(port);
    AuditionUiDispatcher dispatcher(controller);

    AuditionStateSnapshot captured{};
    assertApplied(dispatcher.captureSnapshot(captured));
    assert(captured.audioLoaded);
    assert(captured.volumeMaximum == 1.2f);

    AuditionTransportView view(captured, dispatcher);
    assert(&view.state() == &captured);
    assert(view.footerPlaybackEnabled());

    assertApplied(view.toggleVisibleFooterPlayback());
    assert((port.events == std::vector<std::string>{"footer-playback"}));

    assertApplied(view.setVolume(9.0f));
    assert(port.events.back() == "volume");
    assert(std::abs(port.lastValue - 1.2f) < 0.0001f);

    assertApplied(view.setPitchControlValue(-90.0f));
    assert(port.events.back() == "pitch-value");
    assert(std::abs(port.lastValue - (-24.0f)) < 0.0001f);

    port.snapshot.pitchMode = AuditionPitchMode::FrequencyShift;
    assertApplied(view.setPitchControlValue(9000.0f));
    assert(std::abs(port.lastValue - 5000.0f) < 0.0001f);

    assertApplied(view.changePitchModeAndReset(AuditionPitchMode::Semitone));
    assert(port.events.back() == "pitch-mode-reset");
    assert(port.lastMode == AuditionPitchMode::Semitone);

    assertApplied(view.toggleReversePreservingRegion());
    assert(port.events.back() == "reverse-preserve-region");

    assertApplied(view.setLoopAndRearmRegion(true));
    assert(port.events.back() == "loop-rearm-region");
    assert(port.lastEnabled);

    const std::size_t beforeRejected = port.events.size();
    const auto nanResult = dispatcher.dispatch(
        AuditionAction::setVolume(std::numeric_limits<float>::quiet_NaN()));
    assert(nanResult.status == AuditionDispatchStatus::Rejected);
    assert(nanResult.diagnostic == AuditionDiagnosticCode::NonFinitePayload);
    assert(port.events.size() == beforeRejected);

    const auto infinityResult = dispatcher.dispatch(
        AuditionAction::setPitchControlValue(
            std::numeric_limits<float>::infinity()));
    assert(infinityResult.status == AuditionDispatchStatus::Rejected);
    assert(infinityResult.diagnostic == AuditionDiagnosticCode::NonFinitePayload);
    assert(port.events.size() == beforeRejected);

    port.snapshot.reverseEnabled = false;
    const auto unavailableResult =
        dispatcher.dispatch(AuditionAction::toggleReversePreservingRegion());
    assert(unavailableResult.status == AuditionDispatchStatus::Unavailable);
    assert(unavailableResult.diagnostic == AuditionDiagnosticCode::ActionUnavailable);
    assert(port.events.size() == beforeRejected);
    port.snapshot.reverseEnabled = true;

    port.throwOnIntent = true;
    const auto failedResult = dispatcher.dispatch(AuditionAction::setLoopAndRearmRegion(false));
    assert(failedResult.status == AuditionDispatchStatus::Failed);
    assert(failedResult.diagnostic == AuditionDiagnosticCode::AdapterFailure);
    assert(port.events.size() == beforeRejected);
    port.throwOnIntent = false;

    AuditionAction unknownAction{};
    unknownAction.type = static_cast<aris::frontend::AuditionActionType>(255);
    const auto unknownResult = dispatcher.dispatch(unknownAction);
    assert(unknownResult.status == AuditionDispatchStatus::Rejected);
    assert(unknownResult.diagnostic == AuditionDiagnosticCode::UnknownAction);
    assert(port.events.size() == beforeRejected);

    aris::frontend::AuditionDispatchResult offThreadResult{};
    std::thread wrongThread([&] {
        offThreadResult = dispatcher.dispatch(AuditionAction::setVolume(0.5f));
    });
    wrongThread.join();
    assert(offThreadResult.status == AuditionDispatchStatus::WrongThread);
    assert(offThreadResult.diagnostic == AuditionDiagnosticCode::WrongThread);
    assert(port.events.size() == beforeRejected);

    return 0;
}

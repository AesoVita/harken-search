#include "OriginalAudioWriteConfirmGate.h"

namespace aura::metadata {

OriginalAudioWriteConfirmDecision EvaluateOriginalAudioWriteConfirm(
    OriginalAudioWriteConfirmRequest request) {
    if (request.cancelled) {
        return OriginalAudioWriteConfirmDecision::Cancelled;
    }
    if (request.suppressConfirmDialogs || request.confirmed) {
        return OriginalAudioWriteConfirmDecision::Proceed;
    }
    return OriginalAudioWriteConfirmDecision::NeedPrompt;
}

bool ShouldWriteOriginalAudio(OriginalAudioWriteConfirmDecision decision) {
    return decision == OriginalAudioWriteConfirmDecision::Proceed;
}

}

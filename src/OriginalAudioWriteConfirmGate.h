#pragma once

#include <utility>

namespace aura::metadata {

enum class OriginalAudioWriteConfirmDecision {
    NeedPrompt,
    Proceed,
    Cancelled,
};

struct OriginalAudioWriteConfirmRequest {
    bool suppressConfirmDialogs = false;
    bool confirmed = false;
    bool cancelled = false;
};

OriginalAudioWriteConfirmDecision EvaluateOriginalAudioWriteConfirm(
    OriginalAudioWriteConfirmRequest request);

bool ShouldWriteOriginalAudio(OriginalAudioWriteConfirmDecision decision);

template <typename WriteFn>
bool TryInvokeOriginalAudioWrite(OriginalAudioWriteConfirmRequest request, WriteFn&& write) {
    if (!ShouldWriteOriginalAudio(EvaluateOriginalAudioWriteConfirm(request))) {
        return false;
    }
    write();
    return true;
}

}

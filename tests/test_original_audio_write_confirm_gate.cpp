#include "src/OriginalAudioWriteConfirmGate.h"

#include <cstdio>

namespace {

int g_failures = 0;
int g_writeCalls = 0;

void Check(bool condition, const char* message) {
    if (condition) {
        std::printf("PASS: %s\n", message);
    } else {
        std::printf("FAIL: %s\n", message);
        ++g_failures;
    }
}

void FakeWriteAlbumArt() { ++g_writeCalls; }
void FakeWriteMetadataFields() { ++g_writeCalls; }

}

int main() {
    using aura::metadata::EvaluateOriginalAudioWriteConfirm;
    using aura::metadata::OriginalAudioWriteConfirmDecision;
    using aura::metadata::OriginalAudioWriteConfirmRequest;
    using aura::metadata::ShouldWriteOriginalAudio;
    using aura::metadata::TryInvokeOriginalAudioWrite;

    g_writeCalls = 0;
    auto unconfirmed = EvaluateOriginalAudioWriteConfirm(
        OriginalAudioWriteConfirmRequest{false, false, false});
    Check(unconfirmed == OriginalAudioWriteConfirmDecision::NeedPrompt,
          "unconfirmed request needs a prompt");
    Check(!ShouldWriteOriginalAudio(unconfirmed),
          "unconfirmed request must not write");
    Check(!TryInvokeOriginalAudioWrite(OriginalAudioWriteConfirmRequest{false, false, false},
                                       FakeWriteAlbumArt),
          "unconfirmed request does not invoke writeAlbumArt");
    Check(!TryInvokeOriginalAudioWrite(OriginalAudioWriteConfirmRequest{false, false, false},
                                       FakeWriteMetadataFields),
          "unconfirmed request does not invoke writeMetadataFields");
    Check(g_writeCalls == 0, "no write callbacks fire before confirm");

    g_writeCalls = 0;
    auto confirmed = EvaluateOriginalAudioWriteConfirm(
        OriginalAudioWriteConfirmRequest{false, true, false});
    Check(confirmed == OriginalAudioWriteConfirmDecision::Proceed,
          "confirmed request proceeds");
    Check(TryInvokeOriginalAudioWrite(OriginalAudioWriteConfirmRequest{false, true, false},
                                      FakeWriteAlbumArt),
          "confirmed request invokes writeAlbumArt");
    Check(g_writeCalls == 1, "write callback fires once after confirm");

    g_writeCalls = 0;
    auto suppressed = EvaluateOriginalAudioWriteConfirm(
        OriginalAudioWriteConfirmRequest{true, false, false});
    Check(suppressed == OriginalAudioWriteConfirmDecision::Proceed,
          "suppressConfirmDialogs proceeds without a prompt");
    Check(TryInvokeOriginalAudioWrite(OriginalAudioWriteConfirmRequest{true, false, false},
                                      FakeWriteAlbumArt),
          "suppressConfirmDialogs invokes write directly");
    Check(g_writeCalls == 1, "suppress path uses the same write callback");

    g_writeCalls = 0;
    auto cancelled = EvaluateOriginalAudioWriteConfirm(
        OriginalAudioWriteConfirmRequest{false, false, true});
    Check(cancelled == OriginalAudioWriteConfirmDecision::Cancelled,
          "cancel does not proceed");
    Check(!TryInvokeOriginalAudioWrite(OriginalAudioWriteConfirmRequest{false, false, true},
                                       FakeWriteAlbumArt),
          "cancel does not invoke writeAlbumArt");
    Check(g_writeCalls == 0, "cancel leaves original files untouched");

    std::printf("\n=== Original Audio Write Confirm Gate: %d failure(s) ===\n",
                g_failures);
    return g_failures == 0 ? 0 : 1;
}

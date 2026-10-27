#include "platform/audio_playback.h"
#include "audio_playback_miniaudio_test_seam.h"

#include <cstdint>
#include <iostream>

namespace {
int g_failures = 0;

void Check(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "FAIL: " << message << '\n';
    ++g_failures;
}
} // namespace

int main() {
    using namespace Platform::AudioPlayback;

    TestSeam::Reset();
    Check(QueryCursor(BorrowSound(nullptr)) == 0, "null sound returns frame zero");
    Check(TestSeam::QueryCallCount() == 0, "null sound does not call backend");

    int nativeSound = 0;
    TestSeam::SetQueryResult(0, 4242);
    Check(QueryCursor(BorrowSound(&nativeSound)) == 4242, "successful cursor is returned");
    Check(TestSeam::QueryCallCount() == 1, "successful query calls backend once");

    TestSeam::SetQueryResult(-1, 9999);
    Check(QueryCursor(BorrowSound(&nativeSound)) == 0, "backend failure returns frame zero");
    Check(TestSeam::QueryCallCount() == 2, "failed query calls backend once");

    TestSeam::SetPlayingResult(1);
    Check(IsPlaying(BorrowSound(&nativeSound)), "playing backend state is returned");
    Check(TestSeam::IsPlayingCallCount() == 1, "playing query calls backend once");

    TestSeam::SetPlayingResult(0);
    Check(!IsPlaying(BorrowSound(&nativeSound)), "stopped backend state is returned");
    Check(TestSeam::IsPlayingCallCount() == 2, "stopped query calls backend once");

    TestSeam::SetPlayingResult(2);
    Check(!IsPlaying(BorrowSound(&nativeSound)), "noncanonical backend state is not playing");
    Check(TestSeam::IsPlayingCallCount() == 3, "noncanonical query calls backend once");

    Check(!IsPlaying(BorrowSound(nullptr)), "null sound is not playing");
    Check(TestSeam::IsPlayingCallCount() == 3, "null sound does not call playing backend");

    SetPan(BorrowSound(&nativeSound), -1.25f);
    Check(TestSeam::SetPanCallCount() == 1, "pan update calls backend once");
    Check(TestSeam::LastPanSound() == &nativeSound, "pan update forwards sound identity");
    Check(TestSeam::LastPan() == -1.25f, "pan update forwards value without clamping");

    SetPan(BorrowSound(nullptr), 0.75f);
    Check(TestSeam::SetPanCallCount() == 1, "null sound does not call pan backend");

    SetPitch(BorrowSound(&nativeSound), 0.375f);
    Check(TestSeam::SetPitchCallCount() == 1, "pitch update calls backend once");
    Check(TestSeam::LastPitchSound() == &nativeSound, "pitch update forwards sound identity");
    Check(TestSeam::LastPitch() == 0.375f, "pitch update forwards value without policy changes");

    SetPitch(BorrowSound(nullptr), 2.0f);
    Check(TestSeam::SetPitchCallCount() == 1, "null sound does not call pitch backend");

    SetVolume(BorrowSound(&nativeSound), 1.375f);
    Check(TestSeam::SetVolumeCallCount() == 1, "volume update calls backend once");
    Check(TestSeam::LastVolumeSound() == &nativeSound, "volume update forwards sound identity");
    Check(TestSeam::LastVolume() == 1.375f, "volume update forwards value without policy changes");

    SetVolume(BorrowSound(nullptr), 0.25f);
    Check(TestSeam::SetVolumeCallCount() == 1, "null sound does not call volume backend");

    SetFadeInMilliseconds(BorrowSound(&nativeSound), -1.0f, 0.5f, 37);
    Check(TestSeam::SetFadeInMillisecondsCallCount() == 1, "fade update calls backend once");
    Check(TestSeam::LastFadeSound() == &nativeSound, "fade update forwards sound identity");
    Check(TestSeam::LastFadeVolumeBeg() == -1.0f, "fade update forwards begin volume");
    Check(TestSeam::LastFadeVolumeEnd() == 0.5f, "fade update forwards end volume");
    Check(TestSeam::LastFadeDurationMs() == 37, "fade update forwards duration");

    SetFadeInMilliseconds(BorrowSound(nullptr), 0.0f, 1.0f, 12);
    Check(TestSeam::SetFadeInMillisecondsCallCount() == 1, "null sound does not call fade backend");

    SetFadeInPcmFrames(BorrowSound(&nativeSound), -1.0f, 0.25f, 128);
    Check(TestSeam::SetFadeInPcmFramesCallCount() == 1, "PCM fade update calls backend once");
    Check(TestSeam::LastFadePcmSound() == &nativeSound, "PCM fade update forwards sound identity");
    Check(TestSeam::LastFadePcmVolumeBeg() == -1.0f, "PCM fade update forwards begin volume");
    Check(TestSeam::LastFadePcmVolumeEnd() == 0.25f, "PCM fade update forwards end volume");
    Check(TestSeam::LastFadePcmDurationFrames() == 128, "PCM fade update forwards frame duration");

    SetFadeInPcmFrames(BorrowSound(nullptr), 0.0f, 1.0f, 64);
    Check(TestSeam::SetFadeInPcmFramesCallCount() == 1, "null sound does not call PCM fade backend");

    SetStopTimeInPcmFrames(BorrowSound(&nativeSound), 4096);
    Check(TestSeam::SetStopTimeInPcmFramesCallCount() == 1, "stop time update calls backend once");
    Check(TestSeam::LastStopTimeSound() == &nativeSound, "stop time update forwards sound identity");
    Check(TestSeam::LastStopTimeFrame() == 4096, "stop time update forwards frame");

    SetStopTimeInPcmFrames(BorrowSound(nullptr), 8192);
    Check(TestSeam::SetStopTimeInPcmFramesCallCount() == 1, "null sound does not call stop time backend");

    ResetStopTimeAndFade(BorrowSound(&nativeSound));
    Check(TestSeam::ResetStopTimeAndFadeCallCount() == 1, "stop time and fade reset calls backend once");
    Check(TestSeam::LastResetStopTimeAndFadeSound() == &nativeSound, "stop time and fade reset forwards sound identity");

    ResetStopTimeAndFade(BorrowSound(nullptr));
    Check(TestSeam::ResetStopTimeAndFadeCallCount() == 1, "null sound does not call stop time and fade reset backend");

    TestSeam::SetStartResult(0);
    const auto startSuccess = Start(BorrowSound(&nativeSound));
    Check(startSuccess.Succeeded(), "successful start is reported");
    Check(startSuccess.backendCode == 0, "start success code is preserved");
    Check(TestSeam::StartCallCount() == 1, "start calls backend once");
    Check(TestSeam::LastStartedSound() == &nativeSound, "start forwards sound identity");

    TestSeam::SetStartResult(-7);
    const auto startFailure = Start(BorrowSound(&nativeSound));
    Check(!startFailure.Succeeded(), "failed start is reported");
    Check(startFailure.backendCode == -7, "start failure code is preserved");
    Check(TestSeam::StartCallCount() == 2, "failed start calls backend once");

    const auto invalidStart = Start(BorrowSound(nullptr));
    Check(!invalidStart.Succeeded(), "null sound start is rejected");
    Check(TestSeam::StartCallCount() == 2, "null sound does not call start backend");

    TestSeam::SetStopResult(0);
    const auto stopSuccess = Stop(BorrowSound(&nativeSound));
    Check(stopSuccess.Succeeded(), "successful stop is reported");
    Check(stopSuccess.backendCode == 0, "stop success code is preserved");
    Check(TestSeam::StopCallCount() == 1, "stop calls backend once");
    Check(TestSeam::LastStoppedSound() == &nativeSound, "stop forwards sound identity");

    TestSeam::SetStopResult(-11);
    const auto stopFailure = Stop(BorrowSound(&nativeSound));
    Check(!stopFailure.Succeeded(), "failed stop is reported");
    Check(stopFailure.backendCode == -11, "stop failure code is preserved");
    Check(TestSeam::StopCallCount() == 2, "failed stop calls backend once");

    const auto invalidStop = Stop(BorrowSound(nullptr));
    Check(!invalidStop.Succeeded(), "null sound stop is rejected");
    Check(TestSeam::StopCallCount() == 2, "null sound does not call stop backend");

    TestSeam::SetSeekToFrameResult(0);
    const auto seekSuccess = SeekToFrame(BorrowSound(&nativeSound), 98765);
    Check(seekSuccess.Succeeded(), "successful seek is reported");
    Check(seekSuccess.backendCode == 0, "seek success code is preserved");
    Check(TestSeam::SeekToFrameCallCount() == 1, "seek calls backend once");
    Check(TestSeam::LastSeekSound() == &nativeSound, "seek forwards sound identity");
    Check(TestSeam::LastSeekFrame() == 98765, "seek forwards target frame");

    TestSeam::SetSeekToFrameResult(-13);
    const auto seekFailure = SeekToFrame(BorrowSound(&nativeSound), 123);
    Check(!seekFailure.Succeeded(), "failed seek is reported");
    Check(seekFailure.backendCode == -13, "seek failure code is preserved");
    Check(TestSeam::SeekToFrameCallCount() == 2, "failed seek calls backend once");
    Check(TestSeam::LastSeekFrame() == 123, "failed seek forwards target frame");

    const auto invalidSeek = SeekToFrame(BorrowSound(nullptr), 7);
    Check(!invalidSeek.Succeeded(), "null sound seek is rejected");
    Check(TestSeam::SeekToFrameCallCount() == 2, "null sound does not call seek backend");

    if (g_failures == 0) std::cout << "PASS: audio playback cursor boundary\n";
    return g_failures == 0 ? 0 : 1;
}

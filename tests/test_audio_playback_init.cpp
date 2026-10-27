#include "platform/audio_playback.h"
#include "audio_playback_miniaudio_test_seam.h"
#include "../thirdparty/miniaudio.h"

#include <iostream>
#include <utility>
#include <vector>

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
    int nativeEngine = 0;
    int nativeSource = 0;
    int nativeSound = 0;

    TestSeam::Reset();
    TestSeam::SetInitializeResult(0);
    const auto success = InitializeFromDataSource(
        BorrowEngine(&nativeEngine), BorrowDataSource(&nativeSource),
        UseSoundStorage(&nativeSound), SoundInitFlags::NoSpatialization);
    Check(success.Succeeded(), "successful initialization is reported");
    Check(success.backendCode == 0, "success backend code is preserved");
    Check(TestSeam::InitializeCallCount() == 1, "initialization calls backend once");
    Check(TestSeam::LastEngine() == &nativeEngine, "engine identity is forwarded");
    Check(TestSeam::LastDataSource() == &nativeSource, "data source identity is forwarded");
    Check(TestSeam::LastSound() == &nativeSound, "sound identity is forwarded");
    Check(TestSeam::LastGroup() == nullptr, "sound group remains null");
    Check(TestSeam::LastFlags() == MA_SOUND_FLAG_NO_SPATIALIZATION,
          "no-spatialization flag is mapped");

    TestSeam::SetInitializeResult(-4);
    const auto failure = InitializeFromDataSource(
        BorrowEngine(&nativeEngine), BorrowDataSource(&nativeSource),
        UseSoundStorage(&nativeSound),
        SoundInitFlags::NoSpatialization | SoundInitFlags::NoPitch);
    Check(!failure.Succeeded(), "backend initialization failure is reported");
    Check(failure.backendCode == -4, "failure backend code is preserved");
    Check(TestSeam::LastFlags() ==
              (MA_SOUND_FLAG_NO_SPATIALIZATION | MA_SOUND_FLAG_NO_PITCH),
          "combined initialization flags are mapped");

    const int callsBeforeNull = TestSeam::InitializeCallCount();
    const auto invalidEngine = InitializeFromDataSource(
        BorrowEngine(nullptr), BorrowDataSource(&nativeSource),
        UseSoundStorage(&nativeSound), SoundInitFlags::NoSpatialization);
    Check(!invalidEngine.Succeeded(), "null engine is rejected");
    const auto invalidSource = InitializeFromDataSource(
        BorrowEngine(&nativeEngine), BorrowDataSource(nullptr),
        UseSoundStorage(&nativeSound), SoundInitFlags::NoSpatialization);
    Check(!invalidSource.Succeeded(), "null data source is rejected");
    const auto invalidStorage = InitializeFromDataSource(
        BorrowEngine(&nativeEngine), BorrowDataSource(&nativeSource),
        UseSoundStorage(nullptr), SoundInitFlags::NoSpatialization);
    Check(!invalidStorage.Succeeded(), "null sound storage is rejected");
    Check(TestSeam::InitializeCallCount() == callsBeforeNull,
          "invalid capabilities do not call backend");

    TestSeam::SetEngineSampleRate(96000);
    Check(QueryEngineSampleRate(BorrowEngine(&nativeEngine)) == 96000,
          "engine sample rate query returns backend value");
    Check(TestSeam::QueryEngineSampleRateCallCount() == 1,
          "engine sample rate query calls backend once");
    Check(TestSeam::LastSampleRateEngine() == &nativeEngine,
          "engine sample rate query forwards engine identity");

    Check(QueryEngineSampleRate(BorrowEngine(nullptr)) == 0,
          "null engine sample rate query returns zero");
    Check(TestSeam::QueryEngineSampleRateCallCount() == 1,
          "null engine sample rate query does not call backend");

    TestSeam::SetEngineTimeInPcmFrames(123456);
    Check(QueryEngineTimeInPcmFrames(BorrowEngine(&nativeEngine)) == 123456,
          "engine time query returns backend frame");
    Check(TestSeam::QueryEngineTimeInPcmFramesCallCount() == 1,
          "engine time query calls backend once");
    Check(TestSeam::LastEngineTimeQueryEngine() == &nativeEngine,
          "engine time query forwards engine identity");

    Check(QueryEngineTimeInPcmFrames(BorrowEngine(nullptr)) == 0,
          "null engine time query returns zero");
    Check(TestSeam::QueryEngineTimeInPcmFramesCallCount() == 1,
          "null engine time query does not call backend");

    SetEngineVolume(BorrowEngine(&nativeEngine), 0.125f);
    Check(TestSeam::SetEngineVolumeCallCount() == 1,
          "engine volume update calls backend once");
    Check(TestSeam::LastEngineVolumeEngine() == &nativeEngine,
          "engine volume update forwards engine identity");
    Check(TestSeam::LastEngineVolume() == 0.125f,
          "engine volume update forwards value without policy changes");

    SetEngineVolume(BorrowEngine(nullptr), 0.0f);
    Check(TestSeam::SetEngineVolumeCallCount() == 1,
          "null engine does not call engine volume backend");

    TestSeam::SetPlaybackDeviceMetrics(48000, 256, 3);
    const auto metrics = QueryPlaybackDeviceMetrics(BorrowEngine(&nativeEngine));
    Check(metrics.IsValid(), "playback device metrics query reports valid device");
    Check(metrics.sampleRate == 48000, "playback device metrics query returns sample rate");
    Check(metrics.periodFrames == 256, "playback device metrics query returns period frames");
    Check(metrics.periods == 3, "playback device metrics query returns periods");
    Check(TestSeam::QueryPlaybackDeviceMetricsCallCount() == 1,
          "playback device metrics query calls backend once");
    Check(TestSeam::LastPlaybackDeviceMetricsEngine() == &nativeEngine,
          "playback device metrics query forwards engine identity");

    const auto invalidMetrics = QueryPlaybackDeviceMetrics(BorrowEngine(nullptr));
    Check(!invalidMetrics.IsValid(), "null engine playback device metrics are invalid");
    Check(invalidMetrics.sampleRate == 0, "null engine playback device sample rate is zero");
    Check(TestSeam::QueryPlaybackDeviceMetricsCallCount() == 1,
          "null engine playback device metrics do not call backend");

    TestSeam::SetOutputDevices({
        {"Default Device", true, {0x10, 0x20, 0x30}},
        {"External DAC", false, {0x40, 0x50}},
    });
    const auto outputDevices = EnumerateOutputDevices(BorrowEngine(&nativeEngine));
    Check(outputDevices.size() == 2, "output device enumeration returns backend devices");
    Check(outputDevices[0].name == "Default Device",
          "output device enumeration preserves device name");
    Check(outputDevices[0].isDefault, "output device enumeration preserves default flag");
    Check(outputDevices[0].deviceIdBlob == std::vector<std::uint8_t>({0x10, 0x20, 0x30}),
          "output device enumeration preserves opaque device id blob");
    Check(outputDevices[1].name == "External DAC",
          "output device enumeration preserves second device name");
    Check(!outputDevices[1].isDefault,
          "output device enumeration preserves non-default flag");
    Check(TestSeam::EnumerateOutputDevicesCallCount() == 1,
          "output device enumeration calls backend once");
    Check(TestSeam::LastEnumerateOutputDevicesEngine() == &nativeEngine,
          "output device enumeration forwards engine identity");

    const auto invalidOutputDevices = EnumerateOutputDevices(BorrowEngine(nullptr));
    Check(invalidOutputDevices.empty(), "null engine output device enumeration is empty");
    Check(TestSeam::EnumerateOutputDevicesCallCount() == 1,
          "null engine output device enumeration does not call backend");

    EngineInitOptions engineOptions;
    engineOptions.channels = 2;
    engineOptions.sampleRate = 48000;
    engineOptions.periodSizeMs = 10;
    engineOptions.playbackDeviceIdBlob = {0x01, 0x02, 0x03, 0x04};
    TestSeam::SetEngineInitializeResult(0);
    const auto engineInitSuccess = InitializeEngine(BorrowEngine(&nativeEngine), engineOptions);
    Check(engineInitSuccess.Succeeded(), "engine initialization success is reported");
    Check(engineInitSuccess.backendCode == 0, "engine initialization success code is preserved");
    Check(TestSeam::InitializeEngineCallCount() == 1,
          "engine initialization calls backend once");
    Check(TestSeam::LastInitializedEngine() == &nativeEngine,
          "engine initialization forwards engine identity");
    Check(TestSeam::LastInitializedEngineChannels() == 2,
          "engine initialization forwards channel count");
    Check(TestSeam::LastInitializedEngineSampleRate() == 48000,
          "engine initialization forwards sample rate");
    Check(TestSeam::LastInitializedEnginePeriodSizeMs() == 10,
          "engine initialization forwards period size");
    Check(TestSeam::LastInitializedEngineDeviceIdBlob() ==
              std::vector<std::uint8_t>({0x01, 0x02, 0x03, 0x04}),
          "engine initialization forwards opaque playback device id blob");

    TestSeam::SetEngineInitializeResult(-6);
    engineOptions.playbackDeviceIdBlob.clear();
    const auto engineInitFailure = InitializeEngine(BorrowEngine(&nativeEngine), engineOptions);
    Check(!engineInitFailure.Succeeded(), "engine initialization failure is reported");
    Check(engineInitFailure.backendCode == -6, "engine initialization failure code is preserved");
    Check(TestSeam::InitializeEngineCallCount() == 2,
          "engine initialization failure calls backend once");
    Check(TestSeam::LastInitializedEngineDeviceIdBlob().empty(),
          "engine initialization preserves empty default-device id blob");

    const auto invalidEngineInit = InitializeEngine(BorrowEngine(nullptr), engineOptions);
    Check(!invalidEngineInit.Succeeded(), "null engine initialization fails");
    Check(TestSeam::InitializeEngineCallCount() == 2,
          "null engine initialization does not call backend");

    TestSeam::SetEngineDeviceStopResult(0);
    const auto stopDeviceSuccess = StopEngineDevice(BorrowEngine(&nativeEngine));
    Check(stopDeviceSuccess.Succeeded(), "engine device stop success is reported");
    Check(stopDeviceSuccess.backendCode == 0, "engine device stop success code is preserved");
    Check(TestSeam::StopEngineDeviceCallCount() == 1,
          "engine device stop calls backend once");
    Check(TestSeam::LastStoppedEngineDevice() == &nativeEngine,
          "engine device stop forwards engine identity");

    TestSeam::SetEngineDeviceStopResult(-5);
    const auto stopDeviceFailure = StopEngineDevice(BorrowEngine(&nativeEngine));
    Check(!stopDeviceFailure.Succeeded(), "engine device stop failure is reported");
    Check(stopDeviceFailure.backendCode == -5, "engine device stop failure code is preserved");
    Check(TestSeam::StopEngineDeviceCallCount() == 2,
          "engine device stop failure calls backend once");

    const auto invalidStopDevice = StopEngineDevice(BorrowEngine(nullptr));
    Check(!invalidStopDevice.Succeeded(), "null engine device stop fails");
    Check(TestSeam::StopEngineDeviceCallCount() == 2,
          "null engine device stop does not call backend");

    UninitializeSound(BorrowSound(&nativeSound));
    Check(TestSeam::UninitializeSoundCallCount() == 1,
          "sound uninitialize calls backend once");
    Check(TestSeam::LastUninitializedSound() == &nativeSound,
          "sound uninitialize forwards sound identity");

    UninitializeSound(BorrowSound(nullptr));
    Check(TestSeam::UninitializeSoundCallCount() == 1,
          "null sound uninitialize does not call backend");

    UninitializeEngine(BorrowEngine(&nativeEngine));
    Check(TestSeam::UninitializeEngineCallCount() == 1,
          "engine uninitialize calls backend once");
    Check(TestSeam::LastUninitializedEngine() == &nativeEngine,
          "engine uninitialize forwards engine identity");

    UninitializeEngine(BorrowEngine(nullptr));
    Check(TestSeam::UninitializeEngineCallCount() == 1,
          "null engine uninitialize does not call backend");

    EngineInitOptions ownerOptions;
    ownerOptions.channels = 2;
    ownerOptions.sampleRate = 48000;
    ownerOptions.periodSizeMs = 10;

    TestSeam::Reset();
    TestSeam::SetEngineInitializeResult(0);
    Engine owner;
    const auto ownerInit = owner.Initialize(ownerOptions);
    Check(ownerInit.Succeeded(), "engine owner initializes successfully");
    Check(owner.IsInitialized(), "engine owner reports initialized");
    Check(owner.Borrow().IsValid(), "initialized owner exposes a borrowed engine");
    Check(TestSeam::InitializeEngineCallCount() == 1,
          "engine owner initializes backend once");
    Check(TestSeam::LastInitializedEngine() != nullptr,
          "engine owner passes owned native engine to backend");
    const auto ownerStop = owner.StopDevice();
    Check(ownerStop.Succeeded(), "engine owner forwards device stop");
    Check(TestSeam::StopEngineDeviceCallCount() == 1,
          "engine owner stops device once");
    owner.Uninitialize();
    Check(!owner.IsInitialized(), "engine owner clears initialized state");
    Check(!owner.Borrow().IsValid(), "uninitialized owner exposes no engine");
    Check(TestSeam::UninitializeEngineCallCount() == 1,
          "engine owner uninitializes backend once");
    owner.Uninitialize();
    Check(TestSeam::UninitializeEngineCallCount() == 1,
          "repeated owner uninitialize is idempotent");

    TestSeam::Reset();
    TestSeam::SetEngineInitializeResult(-7);
    Engine failedOwner;
    const auto failedInit = failedOwner.Initialize(ownerOptions);
    Check(!failedInit.Succeeded(), "engine owner reports initialization failure");
    Check(failedInit.backendCode == -7, "engine owner preserves initialization failure code");
    Check(!failedOwner.IsInitialized(), "failed owner remains uninitialized");
    Check(!failedOwner.Borrow().IsValid(), "failed owner exposes no engine");
    Check(TestSeam::UninitializeEngineCallCount() == 0,
          "failed initialization does not uninitialize backend");

    TestSeam::Reset();
    TestSeam::SetEngineInitializeResult(0);
    Engine moveSource;
    Check(moveSource.Initialize(ownerOptions).Succeeded(), "move source initializes");
    Engine moveTarget(std::move(moveSource));
    Check(!moveSource.IsInitialized(), "move construction clears source");
    Check(moveTarget.IsInitialized(), "move construction transfers owner");
    Check(moveTarget.Borrow().IsValid(), "moved owner retains borrowed engine");
    Engine assignedTarget;
    assignedTarget = std::move(moveTarget);
    Check(!moveTarget.IsInitialized(), "move assignment clears source");
    Check(assignedTarget.IsInitialized(), "move assignment transfers owner");
    assignedTarget.Uninitialize();
    Check(TestSeam::UninitializeEngineCallCount() == 1,
          "moved owner uninitializes backend exactly once");

    TestSeam::Reset();
    TestSeam::SetInitializeResult(0);
    Sound soundOwner;
    const auto soundOwnerInit = soundOwner.Initialize(
        BorrowEngine(&nativeEngine), BorrowDataSource(&nativeSource),
        SoundInitFlags::NoSpatialization);
    Check(soundOwnerInit.Succeeded(), "sound owner initializes successfully");
    Check(soundOwner.IsInitialized(), "sound owner reports initialized");
    Check(soundOwner.Borrow().IsValid(), "initialized sound owner exposes a borrowed sound");
    Check(TestSeam::InitializeCallCount() == 1,
          "sound owner initializes backend once");
    Check(TestSeam::LastSound() != nullptr,
          "sound owner passes owned native sound to backend");
    soundOwner.Uninitialize();
    Check(!soundOwner.IsInitialized(), "sound owner clears initialized state");
    Check(!soundOwner.Borrow().IsValid(), "uninitialized sound owner exposes no sound");
    Check(TestSeam::UninitializeSoundCallCount() == 1,
          "sound owner uninitializes backend once");
    soundOwner.Uninitialize();
    Check(TestSeam::UninitializeSoundCallCount() == 1,
          "repeated sound owner uninitialize is idempotent");

    TestSeam::Reset();
    TestSeam::SetInitializeResult(-9);
    Sound failedSoundOwner;
    const auto failedSoundInit = failedSoundOwner.Initialize(
        BorrowEngine(&nativeEngine), BorrowDataSource(&nativeSource),
        SoundInitFlags::NoSpatialization);
    Check(!failedSoundInit.Succeeded(), "sound owner reports initialization failure");
    Check(failedSoundInit.backendCode == -9,
          "sound owner preserves initialization failure code");
    Check(!failedSoundOwner.IsInitialized(), "failed sound owner remains uninitialized");
    Check(!failedSoundOwner.Borrow().IsValid(), "failed sound owner exposes no sound");
    Check(TestSeam::UninitializeSoundCallCount() == 0,
          "failed sound initialization does not uninitialize backend");

    TestSeam::Reset();
    TestSeam::SetInitializeResult(0);
    Sound soundMoveSource;
    Check(soundMoveSource.Initialize(
              BorrowEngine(&nativeEngine), BorrowDataSource(&nativeSource),
              SoundInitFlags::NoSpatialization).Succeeded(),
          "sound move source initializes");
    Sound soundMoveTarget(std::move(soundMoveSource));
    Check(!soundMoveSource.IsInitialized(), "sound move construction clears source");
    Check(soundMoveTarget.IsInitialized(), "sound move construction transfers owner");
    Sound soundAssignedTarget;
    soundAssignedTarget = std::move(soundMoveTarget);
    Check(!soundMoveTarget.IsInitialized(), "sound move assignment clears source");
    Check(soundAssignedTarget.IsInitialized(), "sound move assignment transfers owner");
    soundAssignedTarget.Uninitialize();
    Check(TestSeam::UninitializeSoundCallCount() == 1,
          "moved sound owner uninitializes backend exactly once");

    if (g_failures == 0) std::cout << "PASS: audio playback initialization boundary\n";
    return g_failures == 0 ? 0 : 1;
}

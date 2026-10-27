#include "audio_playback_miniaudio_test_seam.h"

#include <utility>

namespace Platform::AudioPlayback::TestSeam {
namespace {
int g_backendCode = 0;
std::uint64_t g_cursor = 0;
int g_queryCallCount = 0;
std::uint32_t g_playing = 0;
int g_isPlayingCallCount = 0;
int g_setPanCallCount = 0;
void* g_lastPanSound = nullptr;
float g_lastPan = 0.0f;
int g_setPitchCallCount = 0;
void* g_lastPitchSound = nullptr;
float g_lastPitch = 0.0f;
int g_setVolumeCallCount = 0;
void* g_lastVolumeSound = nullptr;
float g_lastVolume = 0.0f;
int g_setEngineVolumeCallCount = 0;
void* g_lastEngineVolumeEngine = nullptr;
float g_lastEngineVolume = 0.0f;
std::uint32_t g_playbackDeviceSampleRate = 0;
std::uint32_t g_playbackDevicePeriodFrames = 0;
std::uint32_t g_playbackDevicePeriods = 0;
int g_queryPlaybackDeviceMetricsCallCount = 0;
void* g_lastPlaybackDeviceMetricsEngine = nullptr;
std::vector<OutputDeviceFixture> g_outputDevices;
int g_enumerateOutputDevicesCallCount = 0;
void* g_lastEnumerateOutputDevicesEngine = nullptr;
int g_engineInitializeBackendCode = 0;
int g_engineInitializeCallCount = 0;
void* g_lastInitializedEngine = nullptr;
std::uint32_t g_lastInitializedEngineChannels = 0;
std::uint32_t g_lastInitializedEngineSampleRate = 0;
std::uint32_t g_lastInitializedEnginePeriodSizeMs = 0;
std::vector<std::uint8_t> g_lastInitializedEngineDeviceIdBlob;
int g_setFadeCallCount = 0;
void* g_lastFadeSound = nullptr;
float g_lastFadeVolumeBeg = 0.0f;
float g_lastFadeVolumeEnd = 0.0f;
std::uint64_t g_lastFadeDurationMs = 0;
int g_setFadePcmCallCount = 0;
void* g_lastFadePcmSound = nullptr;
float g_lastFadePcmVolumeBeg = 0.0f;
float g_lastFadePcmVolumeEnd = 0.0f;
std::uint64_t g_lastFadePcmDurationFrames = 0;
int g_setStopTimeCallCount = 0;
void* g_lastStopTimeSound = nullptr;
std::uint64_t g_lastStopTimeFrame = 0;
int g_resetStopTimeAndFadeCallCount = 0;
void* g_lastResetStopTimeAndFadeSound = nullptr;
int g_startBackendCode = 0;
int g_startCallCount = 0;
void* g_lastStartedSound = nullptr;
int g_stopBackendCode = 0;
int g_stopCallCount = 0;
void* g_lastStoppedSound = nullptr;
int g_engineDeviceStopBackendCode = 0;
int g_engineDeviceStopCallCount = 0;
void* g_lastStoppedEngineDevice = nullptr;
int g_seekBackendCode = 0;
int g_seekCallCount = 0;
void* g_lastSeekSound = nullptr;
std::uint64_t g_lastSeekFrame = 0;
int g_initializeBackendCode = 0;
int g_initializeCallCount = 0;
void* g_lastEngine = nullptr;
void* g_lastDataSource = nullptr;
void* g_lastSound = nullptr;
void* g_lastGroup = nullptr;
std::uint32_t g_lastFlags = 0;
std::uint32_t g_engineSampleRate = 0;
int g_queryEngineSampleRateCallCount = 0;
void* g_lastSampleRateEngine = nullptr;
std::uint64_t g_engineTimeInPcmFrames = 0;
int g_queryEngineTimeInPcmFramesCallCount = 0;
void* g_lastEngineTimeQueryEngine = nullptr;
int g_uninitializeSoundCallCount = 0;
void* g_lastUninitializedSound = nullptr;
int g_uninitializeEngineCallCount = 0;
void* g_lastUninitializedEngine = nullptr;
} // namespace

void SetQueryResult(int backendCode, std::uint64_t cursor) noexcept {
    g_backendCode = backendCode;
    g_cursor = cursor;
}

int QueryCursor(void*, std::uint64_t* cursor) noexcept {
    ++g_queryCallCount;
    if (cursor) *cursor = g_cursor;
    return g_backendCode;
}

int QueryCallCount() noexcept { return g_queryCallCount; }

void SetPlayingResult(std::uint32_t playing) noexcept { g_playing = playing; }

std::uint32_t IsPlaying(void*) noexcept {
    ++g_isPlayingCallCount;
    return g_playing;
}

int IsPlayingCallCount() noexcept { return g_isPlayingCallCount; }

void SetPan(void* sound, float pan) noexcept {
    ++g_setPanCallCount;
    g_lastPanSound = sound;
    g_lastPan = pan;
}

int SetPanCallCount() noexcept { return g_setPanCallCount; }
void* LastPanSound() noexcept { return g_lastPanSound; }
float LastPan() noexcept { return g_lastPan; }

void SetPitch(void* sound, float pitch) noexcept {
    ++g_setPitchCallCount;
    g_lastPitchSound = sound;
    g_lastPitch = pitch;
}

int SetPitchCallCount() noexcept { return g_setPitchCallCount; }
void* LastPitchSound() noexcept { return g_lastPitchSound; }
float LastPitch() noexcept { return g_lastPitch; }

void SetVolume(void* sound, float volume) noexcept {
    ++g_setVolumeCallCount;
    g_lastVolumeSound = sound;
    g_lastVolume = volume;
}

int SetVolumeCallCount() noexcept { return g_setVolumeCallCount; }
void* LastVolumeSound() noexcept { return g_lastVolumeSound; }
float LastVolume() noexcept { return g_lastVolume; }

void SetEngineVolume(void* engine, float volume) noexcept {
    ++g_setEngineVolumeCallCount;
    g_lastEngineVolumeEngine = engine;
    g_lastEngineVolume = volume;
}

int SetEngineVolumeCallCount() noexcept { return g_setEngineVolumeCallCount; }
void* LastEngineVolumeEngine() noexcept { return g_lastEngineVolumeEngine; }
float LastEngineVolume() noexcept { return g_lastEngineVolume; }

void SetPlaybackDeviceMetrics(std::uint32_t sampleRate, std::uint32_t periodFrames,
                              std::uint32_t periods) noexcept {
    g_playbackDeviceSampleRate = sampleRate;
    g_playbackDevicePeriodFrames = periodFrames;
    g_playbackDevicePeriods = periods;
}

bool QueryPlaybackDeviceMetrics(void* engine, std::uint32_t* sampleRate,
                                std::uint32_t* periodFrames, std::uint32_t* periods) noexcept {
    ++g_queryPlaybackDeviceMetricsCallCount;
    g_lastPlaybackDeviceMetricsEngine = engine;
    if (sampleRate) *sampleRate = g_playbackDeviceSampleRate;
    if (periodFrames) *periodFrames = g_playbackDevicePeriodFrames;
    if (periods) *periods = g_playbackDevicePeriods;
    return true;
}

int QueryPlaybackDeviceMetricsCallCount() noexcept {
    return g_queryPlaybackDeviceMetricsCallCount;
}

void* LastPlaybackDeviceMetricsEngine() noexcept { return g_lastPlaybackDeviceMetricsEngine; }

void SetOutputDevices(std::vector<OutputDeviceFixture> devices) noexcept {
    g_outputDevices = std::move(devices);
}

std::vector<OutputDeviceFixture> EnumerateOutputDevices(void* engine) {
    ++g_enumerateOutputDevicesCallCount;
    g_lastEnumerateOutputDevicesEngine = engine;
    return g_outputDevices;
}

int EnumerateOutputDevicesCallCount() noexcept { return g_enumerateOutputDevicesCallCount; }
void* LastEnumerateOutputDevicesEngine() noexcept { return g_lastEnumerateOutputDevicesEngine; }

void SetEngineInitializeResult(int backendCode) noexcept {
    g_engineInitializeBackendCode = backendCode;
}

int InitializeEngine(void* engine, std::uint32_t channels, std::uint32_t sampleRate,
                     std::uint32_t periodSizeMs, const std::vector<std::uint8_t>& deviceIdBlob) noexcept {
    ++g_engineInitializeCallCount;
    g_lastInitializedEngine = engine;
    g_lastInitializedEngineChannels = channels;
    g_lastInitializedEngineSampleRate = sampleRate;
    g_lastInitializedEnginePeriodSizeMs = periodSizeMs;
    g_lastInitializedEngineDeviceIdBlob = deviceIdBlob;
    return g_engineInitializeBackendCode;
}

int InitializeEngineCallCount() noexcept { return g_engineInitializeCallCount; }
void* LastInitializedEngine() noexcept { return g_lastInitializedEngine; }
std::uint32_t LastInitializedEngineChannels() noexcept { return g_lastInitializedEngineChannels; }
std::uint32_t LastInitializedEngineSampleRate() noexcept { return g_lastInitializedEngineSampleRate; }
std::uint32_t LastInitializedEnginePeriodSizeMs() noexcept { return g_lastInitializedEnginePeriodSizeMs; }
std::vector<std::uint8_t> LastInitializedEngineDeviceIdBlob() {
    return g_lastInitializedEngineDeviceIdBlob;
}

void SetFadeInMilliseconds(void* sound, float volumeBeg, float volumeEnd,
                           std::uint64_t durationMs) noexcept {
    ++g_setFadeCallCount;
    g_lastFadeSound = sound;
    g_lastFadeVolumeBeg = volumeBeg;
    g_lastFadeVolumeEnd = volumeEnd;
    g_lastFadeDurationMs = durationMs;
}

int SetFadeInMillisecondsCallCount() noexcept { return g_setFadeCallCount; }
void* LastFadeSound() noexcept { return g_lastFadeSound; }
float LastFadeVolumeBeg() noexcept { return g_lastFadeVolumeBeg; }
float LastFadeVolumeEnd() noexcept { return g_lastFadeVolumeEnd; }
std::uint64_t LastFadeDurationMs() noexcept { return g_lastFadeDurationMs; }

void SetFadeInPcmFrames(void* sound, float volumeBeg, float volumeEnd,
                        std::uint64_t durationFrames) noexcept {
    ++g_setFadePcmCallCount;
    g_lastFadePcmSound = sound;
    g_lastFadePcmVolumeBeg = volumeBeg;
    g_lastFadePcmVolumeEnd = volumeEnd;
    g_lastFadePcmDurationFrames = durationFrames;
}

int SetFadeInPcmFramesCallCount() noexcept { return g_setFadePcmCallCount; }
void* LastFadePcmSound() noexcept { return g_lastFadePcmSound; }
float LastFadePcmVolumeBeg() noexcept { return g_lastFadePcmVolumeBeg; }
float LastFadePcmVolumeEnd() noexcept { return g_lastFadePcmVolumeEnd; }
std::uint64_t LastFadePcmDurationFrames() noexcept { return g_lastFadePcmDurationFrames; }

void SetStopTimeInPcmFrames(void* sound, std::uint64_t frame) noexcept {
    ++g_setStopTimeCallCount;
    g_lastStopTimeSound = sound;
    g_lastStopTimeFrame = frame;
}

int SetStopTimeInPcmFramesCallCount() noexcept { return g_setStopTimeCallCount; }
void* LastStopTimeSound() noexcept { return g_lastStopTimeSound; }
std::uint64_t LastStopTimeFrame() noexcept { return g_lastStopTimeFrame; }

void ResetStopTimeAndFade(void* sound) noexcept {
    ++g_resetStopTimeAndFadeCallCount;
    g_lastResetStopTimeAndFadeSound = sound;
}

int ResetStopTimeAndFadeCallCount() noexcept { return g_resetStopTimeAndFadeCallCount; }
void* LastResetStopTimeAndFadeSound() noexcept { return g_lastResetStopTimeAndFadeSound; }

void SetStartResult(int backendCode) noexcept { g_startBackendCode = backendCode; }

int Start(void* sound) noexcept {
    ++g_startCallCount;
    g_lastStartedSound = sound;
    return g_startBackendCode;
}

int StartCallCount() noexcept { return g_startCallCount; }
void* LastStartedSound() noexcept { return g_lastStartedSound; }

void SetStopResult(int backendCode) noexcept { g_stopBackendCode = backendCode; }

int Stop(void* sound) noexcept {
    ++g_stopCallCount;
    g_lastStoppedSound = sound;
    return g_stopBackendCode;
}

int StopCallCount() noexcept { return g_stopCallCount; }
void* LastStoppedSound() noexcept { return g_lastStoppedSound; }

void SetEngineDeviceStopResult(int backendCode) noexcept {
    g_engineDeviceStopBackendCode = backendCode;
}

int StopEngineDevice(void* engine) noexcept {
    ++g_engineDeviceStopCallCount;
    g_lastStoppedEngineDevice = engine;
    return g_engineDeviceStopBackendCode;
}

int StopEngineDeviceCallCount() noexcept { return g_engineDeviceStopCallCount; }
void* LastStoppedEngineDevice() noexcept { return g_lastStoppedEngineDevice; }

void SetSeekToFrameResult(int backendCode) noexcept { g_seekBackendCode = backendCode; }

int SeekToFrame(void* sound, std::uint64_t frame) noexcept {
    ++g_seekCallCount;
    g_lastSeekSound = sound;
    g_lastSeekFrame = frame;
    return g_seekBackendCode;
}

int SeekToFrameCallCount() noexcept { return g_seekCallCount; }
void* LastSeekSound() noexcept { return g_lastSeekSound; }
std::uint64_t LastSeekFrame() noexcept { return g_lastSeekFrame; }

void SetInitializeResult(int backendCode) noexcept {
    g_initializeBackendCode = backendCode;
}

int InitializeFromDataSource(void* engine, void* dataSource,
                             std::uint32_t flags, void* group, void* sound) noexcept {
    ++g_initializeCallCount;
    g_lastEngine = engine;
    g_lastDataSource = dataSource;
    g_lastFlags = flags;
    g_lastGroup = group;
    g_lastSound = sound;
    return g_initializeBackendCode;
}

int InitializeCallCount() noexcept { return g_initializeCallCount; }
void* LastEngine() noexcept { return g_lastEngine; }
void* LastDataSource() noexcept { return g_lastDataSource; }
void* LastSound() noexcept { return g_lastSound; }
void* LastGroup() noexcept { return g_lastGroup; }
std::uint32_t LastFlags() noexcept { return g_lastFlags; }

void SetEngineSampleRate(std::uint32_t sampleRate) noexcept {
    g_engineSampleRate = sampleRate;
}

std::uint32_t QueryEngineSampleRate(void* engine) noexcept {
    ++g_queryEngineSampleRateCallCount;
    g_lastSampleRateEngine = engine;
    return g_engineSampleRate;
}

int QueryEngineSampleRateCallCount() noexcept {
    return g_queryEngineSampleRateCallCount;
}

void* LastSampleRateEngine() noexcept { return g_lastSampleRateEngine; }

void SetEngineTimeInPcmFrames(std::uint64_t frame) noexcept {
    g_engineTimeInPcmFrames = frame;
}

std::uint64_t QueryEngineTimeInPcmFrames(void* engine) noexcept {
    ++g_queryEngineTimeInPcmFramesCallCount;
    g_lastEngineTimeQueryEngine = engine;
    return g_engineTimeInPcmFrames;
}

int QueryEngineTimeInPcmFramesCallCount() noexcept {
    return g_queryEngineTimeInPcmFramesCallCount;
}

void* LastEngineTimeQueryEngine() noexcept { return g_lastEngineTimeQueryEngine; }

void UninitializeSound(void* sound) noexcept {
    ++g_uninitializeSoundCallCount;
    g_lastUninitializedSound = sound;
}

int UninitializeSoundCallCount() noexcept { return g_uninitializeSoundCallCount; }
void* LastUninitializedSound() noexcept { return g_lastUninitializedSound; }

void UninitializeEngine(void* engine) noexcept {
    ++g_uninitializeEngineCallCount;
    g_lastUninitializedEngine = engine;
}

int UninitializeEngineCallCount() noexcept { return g_uninitializeEngineCallCount; }
void* LastUninitializedEngine() noexcept { return g_lastUninitializedEngine; }

void Reset() noexcept {
    g_backendCode = 0;
    g_cursor = 0;
    g_queryCallCount = 0;
    g_playing = 0;
    g_isPlayingCallCount = 0;
    g_setPanCallCount = 0;
    g_lastPanSound = nullptr;
    g_lastPan = 0.0f;
    g_setPitchCallCount = 0;
    g_lastPitchSound = nullptr;
    g_lastPitch = 0.0f;
    g_setVolumeCallCount = 0;
    g_lastVolumeSound = nullptr;
    g_lastVolume = 0.0f;
    g_setEngineVolumeCallCount = 0;
    g_lastEngineVolumeEngine = nullptr;
    g_lastEngineVolume = 0.0f;
    g_playbackDeviceSampleRate = 0;
    g_playbackDevicePeriodFrames = 0;
    g_playbackDevicePeriods = 0;
    g_queryPlaybackDeviceMetricsCallCount = 0;
    g_lastPlaybackDeviceMetricsEngine = nullptr;
    g_outputDevices.clear();
    g_enumerateOutputDevicesCallCount = 0;
    g_lastEnumerateOutputDevicesEngine = nullptr;
    g_engineInitializeBackendCode = 0;
    g_engineInitializeCallCount = 0;
    g_lastInitializedEngine = nullptr;
    g_lastInitializedEngineChannels = 0;
    g_lastInitializedEngineSampleRate = 0;
    g_lastInitializedEnginePeriodSizeMs = 0;
    g_lastInitializedEngineDeviceIdBlob.clear();
    g_setFadeCallCount = 0;
    g_lastFadeSound = nullptr;
    g_lastFadeVolumeBeg = 0.0f;
    g_lastFadeVolumeEnd = 0.0f;
    g_lastFadeDurationMs = 0;
    g_setFadePcmCallCount = 0;
    g_lastFadePcmSound = nullptr;
    g_lastFadePcmVolumeBeg = 0.0f;
    g_lastFadePcmVolumeEnd = 0.0f;
    g_lastFadePcmDurationFrames = 0;
    g_setStopTimeCallCount = 0;
    g_lastStopTimeSound = nullptr;
    g_lastStopTimeFrame = 0;
    g_resetStopTimeAndFadeCallCount = 0;
    g_lastResetStopTimeAndFadeSound = nullptr;
    g_startBackendCode = 0;
    g_startCallCount = 0;
    g_lastStartedSound = nullptr;
    g_stopBackendCode = 0;
    g_stopCallCount = 0;
    g_lastStoppedSound = nullptr;
    g_engineDeviceStopBackendCode = 0;
    g_engineDeviceStopCallCount = 0;
    g_lastStoppedEngineDevice = nullptr;
    g_seekBackendCode = 0;
    g_seekCallCount = 0;
    g_lastSeekSound = nullptr;
    g_lastSeekFrame = 0;
    g_initializeBackendCode = 0;
    g_initializeCallCount = 0;
    g_lastEngine = nullptr;
    g_lastDataSource = nullptr;
    g_lastSound = nullptr;
    g_lastGroup = nullptr;
    g_lastFlags = 0;
    g_engineSampleRate = 0;
    g_queryEngineSampleRateCallCount = 0;
    g_lastSampleRateEngine = nullptr;
    g_engineTimeInPcmFrames = 0;
    g_queryEngineTimeInPcmFramesCallCount = 0;
    g_lastEngineTimeQueryEngine = nullptr;
    g_uninitializeSoundCallCount = 0;
    g_lastUninitializedSound = nullptr;
    g_uninitializeEngineCallCount = 0;
    g_lastUninitializedEngine = nullptr;
}

} // namespace Platform::AudioPlayback::TestSeam

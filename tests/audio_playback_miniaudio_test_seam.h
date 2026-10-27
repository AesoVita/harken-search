#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Platform::AudioPlayback::TestSeam {

struct OutputDeviceFixture {
    std::string name;
    bool isDefault = false;
    std::vector<std::uint8_t> idBlob;
};

void SetQueryResult(int backendCode, std::uint64_t cursor) noexcept;
int QueryCursor(void* sound, std::uint64_t* cursor) noexcept;
int QueryCallCount() noexcept;
void SetPlayingResult(std::uint32_t playing) noexcept;
std::uint32_t IsPlaying(void* sound) noexcept;
int IsPlayingCallCount() noexcept;
void SetPan(void* sound, float pan) noexcept;
int SetPanCallCount() noexcept;
void* LastPanSound() noexcept;
float LastPan() noexcept;
void SetPitch(void* sound, float pitch) noexcept;
int SetPitchCallCount() noexcept;
void* LastPitchSound() noexcept;
float LastPitch() noexcept;
void SetVolume(void* sound, float volume) noexcept;
int SetVolumeCallCount() noexcept;
void* LastVolumeSound() noexcept;
float LastVolume() noexcept;
void SetEngineVolume(void* engine, float volume) noexcept;
int SetEngineVolumeCallCount() noexcept;
void* LastEngineVolumeEngine() noexcept;
float LastEngineVolume() noexcept;
void SetPlaybackDeviceMetrics(std::uint32_t sampleRate, std::uint32_t periodFrames,
                              std::uint32_t periods) noexcept;
bool QueryPlaybackDeviceMetrics(void* engine, std::uint32_t* sampleRate,
                                std::uint32_t* periodFrames, std::uint32_t* periods) noexcept;
int QueryPlaybackDeviceMetricsCallCount() noexcept;
void* LastPlaybackDeviceMetricsEngine() noexcept;
void SetOutputDevices(std::vector<OutputDeviceFixture> devices) noexcept;
std::vector<OutputDeviceFixture> EnumerateOutputDevices(void* engine);
int EnumerateOutputDevicesCallCount() noexcept;
void* LastEnumerateOutputDevicesEngine() noexcept;
void SetEngineInitializeResult(int backendCode) noexcept;
int InitializeEngine(void* engine, std::uint32_t channels, std::uint32_t sampleRate,
                     std::uint32_t periodSizeMs, const std::vector<std::uint8_t>& deviceIdBlob) noexcept;
int InitializeEngineCallCount() noexcept;
void* LastInitializedEngine() noexcept;
std::uint32_t LastInitializedEngineChannels() noexcept;
std::uint32_t LastInitializedEngineSampleRate() noexcept;
std::uint32_t LastInitializedEnginePeriodSizeMs() noexcept;
std::vector<std::uint8_t> LastInitializedEngineDeviceIdBlob();
void SetFadeInMilliseconds(void* sound, float volumeBeg, float volumeEnd,
                           std::uint64_t durationMs) noexcept;
int SetFadeInMillisecondsCallCount() noexcept;
void* LastFadeSound() noexcept;
float LastFadeVolumeBeg() noexcept;
float LastFadeVolumeEnd() noexcept;
std::uint64_t LastFadeDurationMs() noexcept;
void SetFadeInPcmFrames(void* sound, float volumeBeg, float volumeEnd,
                        std::uint64_t durationFrames) noexcept;
int SetFadeInPcmFramesCallCount() noexcept;
void* LastFadePcmSound() noexcept;
float LastFadePcmVolumeBeg() noexcept;
float LastFadePcmVolumeEnd() noexcept;
std::uint64_t LastFadePcmDurationFrames() noexcept;
void SetStopTimeInPcmFrames(void* sound, std::uint64_t frame) noexcept;
int SetStopTimeInPcmFramesCallCount() noexcept;
void* LastStopTimeSound() noexcept;
std::uint64_t LastStopTimeFrame() noexcept;
void ResetStopTimeAndFade(void* sound) noexcept;
int ResetStopTimeAndFadeCallCount() noexcept;
void* LastResetStopTimeAndFadeSound() noexcept;
void SetStartResult(int backendCode) noexcept;
int Start(void* sound) noexcept;
int StartCallCount() noexcept;
void* LastStartedSound() noexcept;
void SetStopResult(int backendCode) noexcept;
int Stop(void* sound) noexcept;
int StopCallCount() noexcept;
void* LastStoppedSound() noexcept;
void SetEngineDeviceStopResult(int backendCode) noexcept;
int StopEngineDevice(void* engine) noexcept;
int StopEngineDeviceCallCount() noexcept;
void* LastStoppedEngineDevice() noexcept;
void SetSeekToFrameResult(int backendCode) noexcept;
int SeekToFrame(void* sound, std::uint64_t frame) noexcept;
int SeekToFrameCallCount() noexcept;
void* LastSeekSound() noexcept;
std::uint64_t LastSeekFrame() noexcept;
void Reset() noexcept;
void SetInitializeResult(int backendCode) noexcept;
int InitializeFromDataSource(void* engine, void* dataSource,
                             std::uint32_t flags, void* group, void* sound) noexcept;
int InitializeCallCount() noexcept;
void* LastEngine() noexcept;
void* LastDataSource() noexcept;
void* LastSound() noexcept;
void* LastGroup() noexcept;
std::uint32_t LastFlags() noexcept;
void SetEngineSampleRate(std::uint32_t sampleRate) noexcept;
std::uint32_t QueryEngineSampleRate(void* engine) noexcept;
int QueryEngineSampleRateCallCount() noexcept;
void* LastSampleRateEngine() noexcept;
void SetEngineTimeInPcmFrames(std::uint64_t frame) noexcept;
std::uint64_t QueryEngineTimeInPcmFrames(void* engine) noexcept;
int QueryEngineTimeInPcmFramesCallCount() noexcept;
void* LastEngineTimeQueryEngine() noexcept;
void UninitializeSound(void* sound) noexcept;
int UninitializeSoundCallCount() noexcept;
void* LastUninitializedSound() noexcept;
void UninitializeEngine(void* engine) noexcept;
int UninitializeEngineCallCount() noexcept;
void* LastUninitializedEngine() noexcept;

} // namespace Platform::AudioPlayback::TestSeam

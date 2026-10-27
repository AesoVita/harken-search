#include "audio_playback.h"

#include "../../thirdparty/miniaudio.h"
#ifdef AURO_AUDIO_PLAYBACK_TESTING
#include "../../tests/audio_playback_miniaudio_test_seam.h"
#endif

#include <cstring>
#include <utility>

namespace Platform::AudioPlayback {

namespace Detail {
struct Access {
    static void* Native(BorrowedEngine engine) noexcept { return engine.native_; }
    static void* Native(BorrowedDataSource dataSource) noexcept { return dataSource.native_; }
    static void* Native(SoundStorage soundStorage) noexcept { return soundStorage.native_; }
    static void* Native(BorrowedSound sound) noexcept { return sound.native_; }
};
} // namespace Detail

BorrowedSound BorrowSound(void* sound) noexcept { return BorrowedSound(sound); }
BorrowedEngine BorrowEngine(void* engine) noexcept { return BorrowedEngine(engine); }
BorrowedDataSource BorrowDataSource(void* dataSource) noexcept {
    return BorrowedDataSource(dataSource);
}
SoundStorage UseSoundStorage(void* soundStorage) noexcept {
    return SoundStorage(soundStorage);
}

struct Engine::Impl {
    ma_engine* native = nullptr;
    bool initialized = false;
};

Engine::Engine() noexcept = default;

Engine::~Engine() {
    Uninitialize();
}

Engine::Engine(Engine&& other) noexcept = default;

Engine& Engine::operator=(Engine&& other) noexcept {
    if (this == &other) return *this;
    Uninitialize();
    impl_ = std::move(other.impl_);
    return *this;
}

BackendResult Engine::Initialize(const EngineInitOptions& options) {
    if (!impl_) impl_ = std::make_unique<Impl>();
    if (impl_->initialized) return {0};
    impl_->native = new ma_engine();
    const BackendResult result = InitializeEngine(BorrowEngine(impl_->native), options);
    if (!result.Succeeded()) {
        delete impl_->native;
        impl_->native = nullptr;
        return result;
    }
    impl_->initialized = true;
    return result;
}

BorrowedEngine Engine::Borrow() noexcept {
    if (!impl_ || !impl_->initialized) return BorrowEngine(nullptr);
    return BorrowEngine(impl_->native);
}

bool Engine::IsInitialized() const noexcept {
    return impl_ != nullptr && impl_->initialized;
}

BackendResult Engine::StopDevice() noexcept {
    if (!IsInitialized()) return {-1};
    return StopEngineDevice(BorrowEngine(impl_->native));
}

void Engine::Uninitialize() noexcept {
    if (!impl_ || !impl_->native) return;
    if (impl_->initialized) {
        UninitializeEngine(BorrowEngine(impl_->native));
    }
    delete impl_->native;
    impl_->native = nullptr;
    impl_->initialized = false;
}

namespace {
bool HasFlag(SoundInitFlags flags, SoundInitFlags candidate) noexcept {
    return (static_cast<std::uint8_t>(flags) & static_cast<std::uint8_t>(candidate)) != 0;
}
} // namespace

BackendResult InitializeFromDataSource(
    BorrowedEngine engine,
    BorrowedDataSource dataSource,
    SoundStorage soundStorage,
    SoundInitFlags flags) noexcept {
    if (!engine.IsValid() || !dataSource.IsValid() || !soundStorage.IsValid()) return {-1};

    ma_uint32 nativeFlags = 0;
    if (HasFlag(flags, SoundInitFlags::NoSpatialization))
        nativeFlags |= MA_SOUND_FLAG_NO_SPATIALIZATION;
    if (HasFlag(flags, SoundInitFlags::NoPitch))
        nativeFlags |= MA_SOUND_FLAG_NO_PITCH;
#ifdef AURO_AUDIO_PLAYBACK_TESTING
    const int result = TestSeam::InitializeFromDataSource(
        Detail::Access::Native(engine), Detail::Access::Native(dataSource),
        nativeFlags, nullptr, Detail::Access::Native(soundStorage));
#else
    const ma_result result = ma_sound_init_from_data_source(
        static_cast<ma_engine*>(Detail::Access::Native(engine)),
        static_cast<ma_data_source*>(Detail::Access::Native(dataSource)),
        nativeFlags, nullptr,
        static_cast<ma_sound*>(Detail::Access::Native(soundStorage)));
#endif
    return {static_cast<int>(result)};
}

struct Sound::Impl {
    ma_sound* native = nullptr;
    bool initialized = false;
};

Sound::Sound() noexcept = default;

Sound::~Sound() {
    Uninitialize();
}

Sound::Sound(Sound&& other) noexcept = default;

Sound& Sound::operator=(Sound&& other) noexcept {
    if (this == &other) return *this;
    Uninitialize();
    impl_ = std::move(other.impl_);
    return *this;
}

BackendResult Sound::Initialize(
    BorrowedEngine engine,
    BorrowedDataSource dataSource,
    SoundInitFlags flags) {
    if (!impl_) impl_ = std::make_unique<Impl>();
    if (impl_->initialized) return {0};
    impl_->native = new ma_sound();
    const BackendResult result = InitializeFromDataSource(
        engine, dataSource, UseSoundStorage(impl_->native), flags);
    if (!result.Succeeded()) {
        delete impl_->native;
        impl_->native = nullptr;
        return result;
    }
    impl_->initialized = true;
    return result;
}

BorrowedSound Sound::Borrow() noexcept {
    if (!impl_ || !impl_->initialized) return BorrowSound(nullptr);
    return BorrowSound(impl_->native);
}

bool Sound::IsInitialized() const noexcept {
    return impl_ != nullptr && impl_->initialized;
}

void Sound::Uninitialize() noexcept {
    if (!impl_ || !impl_->native) return;
    if (impl_->initialized) {
        UninitializeSound(BorrowSound(impl_->native));
    }
    delete impl_->native;
    impl_->native = nullptr;
    impl_->initialized = false;
}

std::uint32_t QueryEngineSampleRate(BorrowedEngine engine) noexcept {
    if (!engine.IsValid()) return 0;
#ifdef AURO_AUDIO_PLAYBACK_TESTING
    return TestSeam::QueryEngineSampleRate(Detail::Access::Native(engine));
#else
    return static_cast<std::uint32_t>(
        ma_engine_get_sample_rate(static_cast<ma_engine*>(Detail::Access::Native(engine))));
#endif
}

FrameIndex QueryEngineTimeInPcmFrames(BorrowedEngine engine) noexcept {
    if (!engine.IsValid()) return 0;
#ifdef AURO_AUDIO_PLAYBACK_TESTING
    return TestSeam::QueryEngineTimeInPcmFrames(Detail::Access::Native(engine));
#else
    return static_cast<FrameIndex>(
        ma_engine_get_time_in_pcm_frames(static_cast<ma_engine*>(Detail::Access::Native(engine))));
#endif
}

PlaybackDeviceMetrics QueryPlaybackDeviceMetrics(BorrowedEngine engine) noexcept {
    if (!engine.IsValid()) return {};
#ifdef AURO_AUDIO_PLAYBACK_TESTING
    PlaybackDeviceMetrics metrics{};
    const bool ok = TestSeam::QueryPlaybackDeviceMetrics(
        Detail::Access::Native(engine), &metrics.sampleRate,
        &metrics.periodFrames, &metrics.periods);
    return ok ? metrics : PlaybackDeviceMetrics{};
#else
    ma_device* device = ma_engine_get_device(
        static_cast<ma_engine*>(Detail::Access::Native(engine)));
    if (!device) return {};
    return {
        static_cast<std::uint32_t>(device->playback.internalSampleRate),
        static_cast<std::uint32_t>(device->playback.internalPeriodSizeInFrames),
        static_cast<std::uint32_t>(device->playback.internalPeriods)};
#endif
}

std::vector<OutputDeviceDescriptor> EnumerateOutputDevices(BorrowedEngine engine) {
    std::vector<OutputDeviceDescriptor> result;
    if (!engine.IsValid()) return result;
#ifdef AURO_AUDIO_PLAYBACK_TESTING
    const auto fixtures = TestSeam::EnumerateOutputDevices(Detail::Access::Native(engine));
    result.reserve(fixtures.size());
    for (const auto& fixture : fixtures) {
        OutputDeviceDescriptor descriptor;
        descriptor.name = fixture.name;
        descriptor.isDefault = fixture.isDefault;
        descriptor.deviceIdBlob = fixture.idBlob;
        result.push_back(std::move(descriptor));
    }
#else
    ma_device* device = ma_engine_get_device(
        static_cast<ma_engine*>(Detail::Access::Native(engine)));
    if (!device) return result;

    ma_context* context = ma_device_get_context(device);
    if (!context) return result;

    ma_device_info* playbackInfos = nullptr;
    ma_uint32 playbackCount = 0;
    ma_device_info* captureInfos = nullptr;
    ma_uint32 captureCount = 0;
    const ma_result devicesResult = ma_context_get_devices(
        context, &playbackInfos, &playbackCount, &captureInfos, &captureCount);
    if (devicesResult != MA_SUCCESS) return result;

    result.reserve(static_cast<std::size_t>(playbackCount));
    for (ma_uint32 index = 0; index < playbackCount; ++index) {
        OutputDeviceDescriptor descriptor;
        descriptor.name = playbackInfos[index].name;
        descriptor.isDefault = playbackInfos[index].isDefault != 0;
        descriptor.deviceIdBlob.resize(sizeof(ma_device_id));
        std::memcpy(
            descriptor.deviceIdBlob.data(),
            &playbackInfos[index].id,
            sizeof(ma_device_id));
        result.push_back(std::move(descriptor));
    }
#endif
    return result;
}

BackendResult InitializeEngine(BorrowedEngine engine, const EngineInitOptions& options) noexcept {
    if (!engine.IsValid()) return {-1};
#ifdef AURO_AUDIO_PLAYBACK_TESTING
    const int result = TestSeam::InitializeEngine(
        Detail::Access::Native(engine),
        options.channels,
        options.sampleRate,
        options.periodSizeMs,
        options.playbackDeviceIdBlob);
#else
    ma_engine_config config = ma_engine_config_init();
    config.channels = options.channels;
    config.sampleRate = options.sampleRate;
    if (options.periodSizeMs > 0) {
        config.periodSizeInMilliseconds = options.periodSizeMs;
    }

    ma_device_id targetDeviceId{};
    if (options.playbackDeviceIdBlob.size() == sizeof(ma_device_id)) {
        std::memcpy(
            &targetDeviceId,
            options.playbackDeviceIdBlob.data(),
            sizeof(ma_device_id));
        config.pPlaybackDeviceID = &targetDeviceId;
    }

    const ma_result result = ma_engine_init(
        &config, static_cast<ma_engine*>(Detail::Access::Native(engine)));
#endif
    return {static_cast<int>(result)};
}

BackendResult StopEngineDevice(BorrowedEngine engine) noexcept {
    if (!engine.IsValid()) return {-1};
#ifdef AURO_AUDIO_PLAYBACK_TESTING
    const int result = TestSeam::StopEngineDevice(Detail::Access::Native(engine));
#else
    ma_device* device = ma_engine_get_device(
        static_cast<ma_engine*>(Detail::Access::Native(engine)));
    if (!device) return {-1};
    const ma_result result = ma_device_stop(device);
#endif
    return {static_cast<int>(result)};
}

void UninitializeEngine(BorrowedEngine engine) noexcept {
    if (!engine.IsValid()) return;
#ifdef AURO_AUDIO_PLAYBACK_TESTING
    TestSeam::UninitializeEngine(Detail::Access::Native(engine));
#else
    ma_engine_uninit(static_cast<ma_engine*>(Detail::Access::Native(engine)));
#endif
}

FrameIndex QueryCursor(BorrowedSound sound) noexcept {
    if (!sound.IsValid()) return 0;

    ma_uint64 cursor = 0;
#ifdef AURO_AUDIO_PLAYBACK_TESTING
    const ma_result result = static_cast<ma_result>(
        TestSeam::QueryCursor(Detail::Access::Native(sound), &cursor));
#else
    const ma_result result = ma_sound_get_cursor_in_pcm_frames(
        static_cast<ma_sound*>(Detail::Access::Native(sound)), &cursor);
#endif
    return result == MA_SUCCESS ? static_cast<FrameIndex>(cursor) : 0;
}

bool IsPlaying(BorrowedSound sound) noexcept {
    if (!sound.IsValid()) return false;
#ifdef AURO_AUDIO_PLAYBACK_TESTING
    const ma_bool32 playing = static_cast<ma_bool32>(
        TestSeam::IsPlaying(Detail::Access::Native(sound)));
#else
    const ma_bool32 playing = ma_sound_is_playing(
        static_cast<ma_sound*>(Detail::Access::Native(sound)));
#endif
    return playing == MA_TRUE;
}

void SetEngineVolume(BorrowedEngine engine, float volume) noexcept {
    if (!engine.IsValid()) return;
#ifdef AURO_AUDIO_PLAYBACK_TESTING
    TestSeam::SetEngineVolume(Detail::Access::Native(engine), volume);
#else
    ma_engine_set_volume(static_cast<ma_engine*>(Detail::Access::Native(engine)), volume);
#endif
}

void SetPan(BorrowedSound sound, float pan) noexcept {
    if (!sound.IsValid()) return;
#ifdef AURO_AUDIO_PLAYBACK_TESTING
    TestSeam::SetPan(Detail::Access::Native(sound), pan);
#else
    ma_sound_set_pan(static_cast<ma_sound*>(Detail::Access::Native(sound)), pan);
#endif
}

void SetPitch(BorrowedSound sound, float pitch) noexcept {
    if (!sound.IsValid()) return;
#ifdef AURO_AUDIO_PLAYBACK_TESTING
    TestSeam::SetPitch(Detail::Access::Native(sound), pitch);
#else
    ma_sound_set_pitch(static_cast<ma_sound*>(Detail::Access::Native(sound)), pitch);
#endif
}

void SetVolume(BorrowedSound sound, float volume) noexcept {
    if (!sound.IsValid()) return;
#ifdef AURO_AUDIO_PLAYBACK_TESTING
    TestSeam::SetVolume(Detail::Access::Native(sound), volume);
#else
    ma_sound_set_volume(static_cast<ma_sound*>(Detail::Access::Native(sound)), volume);
#endif
}

void SetFadeInMilliseconds(
    BorrowedSound sound,
    float volumeBeg,
    float volumeEnd,
    std::uint64_t durationMs) noexcept {
    if (!sound.IsValid()) return;
#ifdef AURO_AUDIO_PLAYBACK_TESTING
    TestSeam::SetFadeInMilliseconds(
        Detail::Access::Native(sound), volumeBeg, volumeEnd, durationMs);
#else
    ma_sound_set_fade_in_milliseconds(
        static_cast<ma_sound*>(Detail::Access::Native(sound)),
        volumeBeg, volumeEnd, static_cast<ma_uint64>(durationMs));
#endif
}

void SetFadeInPcmFrames(
    BorrowedSound sound,
    float volumeBeg,
    float volumeEnd,
    std::uint64_t durationFrames) noexcept {
    if (!sound.IsValid()) return;
#ifdef AURO_AUDIO_PLAYBACK_TESTING
    TestSeam::SetFadeInPcmFrames(
        Detail::Access::Native(sound), volumeBeg, volumeEnd, durationFrames);
#else
    ma_sound_set_fade_in_pcm_frames(
        static_cast<ma_sound*>(Detail::Access::Native(sound)),
        volumeBeg, volumeEnd, static_cast<ma_uint64>(durationFrames));
#endif
}

void SetStopTimeInPcmFrames(BorrowedSound sound, FrameIndex frame) noexcept {
    if (!sound.IsValid()) return;
#ifdef AURO_AUDIO_PLAYBACK_TESTING
    TestSeam::SetStopTimeInPcmFrames(Detail::Access::Native(sound), frame);
#else
    ma_sound_set_stop_time_in_pcm_frames(
        static_cast<ma_sound*>(Detail::Access::Native(sound)),
        static_cast<ma_uint64>(frame));
#endif
}

void ResetStopTimeAndFade(BorrowedSound sound) noexcept {
    if (!sound.IsValid()) return;
#ifdef AURO_AUDIO_PLAYBACK_TESTING
    TestSeam::ResetStopTimeAndFade(Detail::Access::Native(sound));
#else
    ma_sound_reset_stop_time_and_fade(
        static_cast<ma_sound*>(Detail::Access::Native(sound)));
#endif
}

void UninitializeSound(BorrowedSound sound) noexcept {
    if (!sound.IsValid()) return;
#ifdef AURO_AUDIO_PLAYBACK_TESTING
    TestSeam::UninitializeSound(Detail::Access::Native(sound));
#else
    ma_sound_uninit(static_cast<ma_sound*>(Detail::Access::Native(sound)));
#endif
}

BackendResult Start(BorrowedSound sound) noexcept {
    if (!sound.IsValid()) return {-1};
#ifdef AURO_AUDIO_PLAYBACK_TESTING
    const int result = TestSeam::Start(Detail::Access::Native(sound));
#else
    const ma_result result = ma_sound_start(
        static_cast<ma_sound*>(Detail::Access::Native(sound)));
#endif
    return {static_cast<int>(result)};
}

BackendResult Stop(BorrowedSound sound) noexcept {
    if (!sound.IsValid()) return {-1};
#ifdef AURO_AUDIO_PLAYBACK_TESTING
    const int result = TestSeam::Stop(Detail::Access::Native(sound));
#else
    const ma_result result = ma_sound_stop(
        static_cast<ma_sound*>(Detail::Access::Native(sound)));
#endif
    return {static_cast<int>(result)};
}

BackendResult SeekToFrame(BorrowedSound sound, FrameIndex frame) noexcept {
    if (!sound.IsValid()) return {-1};
#ifdef AURO_AUDIO_PLAYBACK_TESTING
    const int result = TestSeam::SeekToFrame(Detail::Access::Native(sound), frame);
#else
    const ma_result result = ma_sound_seek_to_pcm_frame(
        static_cast<ma_sound*>(Detail::Access::Native(sound)),
        static_cast<ma_uint64>(frame));
#endif
    return {static_cast<int>(result)};
}

} // namespace Platform::AudioPlayback

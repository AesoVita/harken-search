#include "platform/audio_data_source.h"
#include "../thirdparty/miniaudio.h"

#include <cstdint>
#include <iostream>
#include <type_traits>
#include <utility>

namespace {
int g_failures = 0;
ma_uint64 g_lastSeekFrame = 0;

void Check(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "FAIL: " << message << '\n';
    ++g_failures;
}

ma_result Read(ma_data_source*, void* output, ma_uint64 requested, ma_uint64* framesRead) {
    if (framesRead) *framesRead = requested;
    if (output) {
        auto* samples = static_cast<float*>(output);
        for (ma_uint64 i = 0; i < requested; ++i) samples[i] = static_cast<float>(i + 1);
    }
    return MA_SUCCESS;
}
ma_result Seek(ma_data_source*, ma_uint64 frame) {
    g_lastSeekFrame = frame;
    return MA_SUCCESS;
}
ma_result Format(ma_data_source*, ma_format* format, ma_uint32* channels,
                 ma_uint32* sampleRate, ma_channel*, size_t) {
    if (format) *format = ma_format_f32;
    if (channels) *channels = 6;
    if (sampleRate) *sampleRate = 96000;
    return MA_SUCCESS;
}
ma_result Cursor(ma_data_source*, ma_uint64* cursor) { *cursor = 12345; return MA_SUCCESS; }
ma_result Length(ma_data_source*, ma_uint64* length) { *length = 67890; return MA_SUCCESS; }
ma_result Loop(ma_data_source*, ma_bool32) { return MA_SUCCESS; }

const ma_data_source_vtable kVtable{Read, Seek, Format, Cursor, Length, Loop, 0};
} // namespace

int main() {
    using namespace Platform::AudioDataSource;

    using BorrowedCleanup = void (*)(BorrowedDecoder) noexcept;
    constexpr auto borrowedCleanup = static_cast<BorrowedCleanup>(&UninitializeDecoder);
    static_assert(
        std::is_invocable_v<decltype(borrowedCleanup), BorrowedDecoder>,
        "decoder cleanup must accept only the decoder capability");
    static_assert(
        !std::is_invocable_v<decltype(borrowedCleanup), BorrowedSource>,
        "generic data sources must not be accepted by decoder cleanup");
    static_assert(!std::is_copy_constructible_v<Decoder>,
                  "owned decoder must remain move-only");
    static_assert(std::is_move_constructible_v<Decoder>,
                  "owned decoder must support transfer of ownership");

    using OwnedChannelMapInit = OperationResult (*)(
        Decoder&, const std::string&, std::uint32_t, std::uint32_t, int) noexcept;
    constexpr auto ownedChannelMapInit = static_cast<OwnedChannelMapInit>(
        &InitializeDecoderFromFileWithFormatAndChannelMap);

    Decoder ownedDecoder;
    Check(!ownedDecoder.IsInitialized(), "owned decoder starts uninitialized");
    Check(!ownedChannelMapInit(ownedDecoder, "", 2, 48000, 1).Succeeded(),
          "owned channel-map decoder rejects an empty path");
    Check(!InitializeDecoderFromFile(ownedDecoder, "").Succeeded(),
          "owned decoder rejects an empty path");
    Check(!ownedDecoder.IsInitialized(),
          "failed owned decoder initialization remains uninitialized");
    UninitializeDecoder(ownedDecoder);

    ma_data_source_base nativeSource{};
    auto config = ma_data_source_config_init();
    config.vtable = &kVtable;
    Check(ma_data_source_init(&config, &nativeSource) == MA_SUCCESS,
          "fixture data source initializes");

    const auto format = QueryDataFormat(BorrowDataSource(&nativeSource));
    Check(format.Succeeded(), "data source format query reports backend success");
    Check(format.channels == 6, "data source format query returns channel count");
    Check(format.sampleRate == 96000, "data source format query returns sample rate");

    const auto invalid = QueryDataFormat(BorrowDataSource(nullptr));
    Check(!invalid.Succeeded(), "null data source format query fails");
    Check(invalid.channels == 0, "null data source format query returns zero channels");
    Check(invalid.sampleRate == 0, "null data source format query returns zero sample rate");

    const auto cursor = QueryCursor(BorrowDataSource(&nativeSource));
    Check(cursor.Succeeded(), "data source cursor query reports backend success");
    Check(cursor.frame == 12345,
          "data source cursor query returns backend cursor");
    const auto invalidCursor = QueryCursor(BorrowDataSource(nullptr));
    Check(!invalidCursor.Succeeded(), "null data source cursor query fails");
    Check(invalidCursor.frame == 0, "null data source cursor query returns zero");

    const auto length = QueryLength(BorrowDataSource(&nativeSource));
    Check(length.Succeeded(), "data source length query reports backend success");
    Check(length.frames == 67890, "data source length query returns backend frame count");
    const auto invalidLength = QueryLength(BorrowDataSource(nullptr));
    Check(!invalidLength.Succeeded(), "null data source length query fails");
    Check(invalidLength.frames == 0, "null data source length query returns zero");

    float readBuffer[8]{};
    const auto read = ReadPcmFrames(BorrowDataSource(&nativeSource), readBuffer, 8);
    Check(read.backendCode == 0, "data source read reports backend success");
    Check(read.framesRead == 8, "data source read returns backend frame count");
    Check(readBuffer[0] == 1.0f && readBuffer[7] == 8.0f,
          "data source read fills output buffer");
    const auto invalidRead = ReadPcmFrames(BorrowDataSource(nullptr), readBuffer, 8);
    Check(invalidRead.backendCode != 0, "null data source read fails");
    Check(invalidRead.framesRead == 0, "null data source read returns zero frames");

    const auto seek = SeekPcmFrame(BorrowDataSource(&nativeSource), 321);
    Check(seek.backendCode == 0, "data source seek reports backend success");
    Check(g_lastSeekFrame == 321, "data source seek passes frame to backend");
    const auto invalidSeek = SeekPcmFrame(BorrowDataSource(nullptr), 321);
    Check(invalidSeek.backendCode != 0, "null data source seek fails");

    UninitializeDecoder(BorrowDecoder(nullptr));

    static constexpr unsigned char kMonoWav[] = {
        'R', 'I', 'F', 'F', 38, 0, 0, 0,
        'W', 'A', 'V', 'E', 'f', 'm', 't', ' ',
        16, 0, 0, 0, 1, 0, 1, 0,
        0x80, 0xBB, 0, 0, 0, 0x77, 0x01, 0,
        2, 0, 16, 0, 'd', 'a', 't', 'a',
        2, 0, 0, 0, 0, 0,
    };
    ma_decoder decoder{};
    const auto decoderConfig = ma_decoder_config_init(ma_format_f32, 1, 48000);
    Check(
        ma_decoder_init_memory(
            kMonoWav, sizeof(kMonoWav), &decoderConfig, &decoder) == MA_SUCCESS,
        "fixture decoder initializes");
    UninitializeDecoder(BorrowDecoder(&decoder));

    ma_audio_buffer pcmBuffer{};
    float pcmSamples[4]{0.25f, -0.5f, 0.75f, -1.0f};
    const auto pcmInit = InitializePcmBuffer(&pcmBuffer, 2, 2, pcmSamples);
    Check(pcmInit.backendCode == 0, "pcm buffer init reports backend success");
    float pcmRead[4]{};
    const auto pcmReadResult = ReadPcmFrames(BorrowDataSource(&pcmBuffer), pcmRead, 2);
    Check(pcmReadResult.backendCode == 0, "pcm buffer read reports backend success");
    Check(pcmReadResult.framesRead == 2, "pcm buffer read returns frame count");
    Check(pcmRead[0] == 0.25f && pcmRead[3] == -1.0f,
          "pcm buffer read preserves sample data");
    UninitializePcmBuffer(&pcmBuffer);

    static_assert(!std::is_copy_constructible_v<PcmBuffer>,
                  "owned pcm buffer must remain move-only");
    static_assert(std::is_move_constructible_v<PcmBuffer>,
                  "owned pcm buffer must support transfer of ownership");
    PcmBuffer ownedPcm;
    Check(InitializePcmBuffer(ownedPcm, 2, 2, pcmSamples).Succeeded(),
          "owned pcm buffer initializes");
    Check(ownedPcm.IsInitialized(), "owned pcm buffer tracks initialization");
    float ownedRead[4]{};
    const auto ownedReadResult = ReadPcmFrames(BorrowDataSource(ownedPcm), ownedRead, 2);
    Check(ownedReadResult.backendCode == 0 && ownedRead[3] == -1.0f,
          "owned pcm buffer exposes a readable borrowed source");
    PcmBuffer replacementPcm;
    Check(InitializePcmBuffer(replacementPcm, 2, 2, pcmSamples).Succeeded(),
          "replacement pcm buffer initializes");
    ownedPcm = std::move(replacementPcm);
    Check(ownedPcm.IsInitialized(), "move assignment transfers pcm ownership");
    Check(!replacementPcm.IsInitialized(), "moved-from pcm buffer is inert");
    UninitializePcmBuffer(ownedPcm);
    Check(!ownedPcm.IsInitialized(), "owned pcm buffer explicit cleanup is idempotent");

    const auto invalidPcmInit = InitializePcmBuffer(nullptr, 2, 2, pcmSamples);
    Check(invalidPcmInit.backendCode != 0, "null pcm buffer init fails");
    UninitializePcmBuffer(nullptr);

    ma_data_source_uninit(&nativeSource);

    if (g_failures == 0) std::cout << "PASS: audio data source format boundary\n";
    return g_failures == 0 ? 0 : 1;
}

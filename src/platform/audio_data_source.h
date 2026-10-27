#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace Platform::AudioDataSource {

// @stage7-contract
// API contract: borrowed capabilities never own decoder or PCM storage.
// Decoder/PcmBuffer objects own their native state, are move-only, and require
// serialized access. UTF-8 file paths are consumed during initialization;
// result structs preserve backend status and report failure without throwing.

using FrameCount = std::uint64_t;

struct ReadResult {
    int backendCode = 0;
    FrameCount framesRead = 0;
};

struct OperationResult {
    int backendCode = -1;
    bool Succeeded() const noexcept { return backendCode == 0; }
};

struct DataFormat {
    int backendCode = -1;
    std::uint32_t channels = 0;
    std::uint32_t sampleRate = 0;
    bool Succeeded() const noexcept { return backendCode == 0; }
};

struct CursorPosition {
    int backendCode = -1;
    FrameCount frame = 0;
    bool Succeeded() const noexcept { return backendCode == 0; }
};

struct LengthInFrames {
    int backendCode = -1;
    FrameCount frames = 0;
    bool Succeeded() const noexcept { return backendCode == 0; }
};

namespace Detail { struct Access; }

class BorrowedDecoder;
class Decoder;
class PcmBuffer;

class BorrowedSource final {
public:
    bool IsValid() const noexcept { return native_ != nullptr; }

private:
    explicit BorrowedSource(void* native) noexcept : native_(native) {}
    void* native_ = nullptr;
    friend struct Detail::Access;
#ifdef AURO_TEST_EXTERNAL_QUERY_LENGTH
    friend LengthInFrames QueryLength(BorrowedSource source) noexcept;
#endif
    friend class BorrowedDecoder;
    friend BorrowedSource BorrowDataSource(void*) noexcept;
    friend BorrowedSource BorrowDataSource(Decoder&) noexcept;
    friend BorrowedSource BorrowDataSource(PcmBuffer&) noexcept;
};

class BorrowedDecoder final {
public:
    bool IsValid() const noexcept { return native_ != nullptr; }
    operator BorrowedSource() const noexcept { return BorrowedSource(native_); }

private:
    explicit BorrowedDecoder(void* native) noexcept : native_(native) {}
    void* native_ = nullptr;
    friend struct Detail::Access;
    friend BorrowedDecoder BorrowDecoder(void*) noexcept;
    friend BorrowedDecoder BorrowDecoder(Decoder&) noexcept;
};

class Decoder final {
public:
    Decoder();
    ~Decoder();
    Decoder(Decoder&&) noexcept;
    Decoder& operator=(Decoder&&) noexcept;
    Decoder(const Decoder&) = delete;
    Decoder& operator=(const Decoder&) = delete;
    bool IsInitialized() const noexcept;
    void* NativeHandle() noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    friend struct Detail::Access;
    friend BorrowedSource BorrowDataSource(Decoder&) noexcept;
    friend BorrowedDecoder BorrowDecoder(Decoder&) noexcept;
    friend void UninitializeDecoder(Decoder&) noexcept;
};

class PcmBuffer final {
public:
    PcmBuffer();
    ~PcmBuffer();
    PcmBuffer(PcmBuffer&&) noexcept;
    PcmBuffer& operator=(PcmBuffer&&) noexcept;
    PcmBuffer(const PcmBuffer&) = delete;
    PcmBuffer& operator=(const PcmBuffer&) = delete;
    bool IsInitialized() const noexcept;
    void* NativeHandle() noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    friend struct Detail::Access;
    friend BorrowedSource BorrowDataSource(PcmBuffer&) noexcept;
    friend OperationResult InitializePcmBuffer(
        PcmBuffer&, std::uint32_t, FrameCount, const float*) noexcept;
    friend void UninitializePcmBuffer(PcmBuffer&) noexcept;
};

// Borrowed capabilities never own storage. Decoder cleanup is explicit and
// must be performed with UninitializeDecoder before the caller releases it.
BorrowedSource BorrowDataSource(void* source) noexcept;
BorrowedDecoder BorrowDecoder(void* decoder) noexcept;
BorrowedSource BorrowDataSource(Decoder& decoder) noexcept;
BorrowedSource BorrowDataSource(PcmBuffer& buffer) noexcept;
BorrowedDecoder BorrowDecoder(Decoder& decoder) noexcept;
DataFormat QueryDataFormat(BorrowedSource source) noexcept;
CursorPosition QueryCursor(BorrowedSource source) noexcept;
LengthInFrames QueryLength(BorrowedSource source) noexcept;
ReadResult ReadPcmFrames(
    BorrowedSource source,
    float* output,
    FrameCount requestedFrames) noexcept;
OperationResult SeekPcmFrame(BorrowedSource source, FrameCount frame) noexcept;
OperationResult InitializeDecoderFromFile(
    void* decoderStorage, const std::string& pathUtf8) noexcept;
OperationResult InitializeDecoderFromFile(
    Decoder& decoder, const std::string& pathUtf8) noexcept;
OperationResult InitializeDecoderFromFileWithFormat(
    void* decoderStorage,
    const std::string& pathUtf8,
    std::uint32_t channels,
    std::uint32_t sampleRate) noexcept;
OperationResult InitializeDecoderFromFileWithFormat(
    Decoder& decoder,
    const std::string& pathUtf8,
    std::uint32_t channels,
    std::uint32_t sampleRate) noexcept;
OperationResult InitializeDecoderFromFileWithFormatAndChannelMap(
    void* decoderStorage,
    const std::string& pathUtf8,
    std::uint32_t channels,
    std::uint32_t sampleRate,
    int channelSoloMode) noexcept;
OperationResult InitializeDecoderFromFileWithFormatAndChannelMap(
    Decoder& decoder,
    const std::string& pathUtf8,
    std::uint32_t channels,
    std::uint32_t sampleRate,
    int channelSoloMode) noexcept;
void UninitializeDecoder(BorrowedDecoder decoder) noexcept;
void UninitializeDecoder(Decoder& decoder) noexcept;
OperationResult InitializePcmBuffer(
    PcmBuffer& buffer,
    std::uint32_t channels,
    FrameCount frameCount,
    const float* samples) noexcept;
OperationResult InitializePcmBuffer(
    void* bufferStorage,
    std::uint32_t channels,
    FrameCount frameCount,
    const float* samples) noexcept;
void UninitializePcmBuffer(void* bufferStorage) noexcept;
void UninitializePcmBuffer(PcmBuffer& buffer) noexcept;

class Reader final {
public:
    ReadResult Read(float* output, FrameCount requestedFrames) const;
    std::uint32_t Channels() const;

private:
    explicit Reader(void* native) noexcept : native_(native) {}
    void* native_ = nullptr;
    friend struct Detail::Access;
};

struct TransformCallbacks {
    void* context = nullptr;
    ReadResult (*read)(void*, Reader, float*, FrameCount) = nullptr;
    void (*seekSucceeded)(void*) = nullptr;
};

class TransformSource final {
public:
    TransformSource() noexcept;
    ~TransformSource();
    TransformSource(TransformSource&&) noexcept;
    TransformSource& operator=(TransformSource&&) noexcept;
    TransformSource(const TransformSource&) = delete;
    TransformSource& operator=(const TransformSource&) = delete;

    bool Initialize(BorrowedSource source, TransformCallbacks callbacks);
    void Uninitialize() noexcept;
    void* NativeHandle() noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    friend struct Detail::Access;
};

} // namespace Platform::AudioDataSource

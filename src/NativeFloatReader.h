#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

// @stage7-contract
// API contract: NativeFloatReader owns its move-only Backend and closes it on
// destruction/close. open/read/seek are synchronous; path and destination
// buffers are borrowed for each call, optional cancellation is observed but
// never retained, and results report Frames/End/Error/Cancelled without
// throwing across the decoder boundary.

class NativeFloatReader {
public:
    enum class ReadStatus { Frames, End, Error, Cancelled };

    struct Metadata {
        uint32_t sampleRate = 0;
        uint32_t channels = 0;
        uint64_t reportedFrames = 0;
        bool hasReportedFrames = false;
    };

    struct ReadResult {
        ReadStatus status = ReadStatus::End;
        uint64_t framesRead = 0;
        std::string error;
    };

    class Backend {
    public:
        virtual ~Backend() = default;
        virtual Metadata probe() const = 0;
        virtual ReadResult read(float* dst, uint64_t capacityFrames) = 0;
        virtual ReadResult seek(uint64_t frame) = 0;
        virtual void close() = 0;
    };

    NativeFloatReader() = default;
    explicit NativeFloatReader(std::unique_ptr<Backend> backend);
    ~NativeFloatReader();
    NativeFloatReader(NativeFloatReader&&) noexcept = default;
    NativeFloatReader& operator=(NativeFloatReader&&) noexcept = default;
    NativeFloatReader(const NativeFloatReader&) = delete;
    NativeFloatReader& operator=(const NativeFloatReader&) = delete;

    static NativeFloatReader ForTest(std::unique_ptr<Backend> backend);
    static bool IsPathSupported(const std::string& path);

    bool open(const std::string& path);
    ReadResult read(float* interleaved, uint64_t capacityFrames,
                    const std::atomic<bool>* cancelFlag = nullptr);
    ReadResult seek(uint64_t frame,
                    const std::atomic<bool>* cancelFlag = nullptr);
    void close();

    const Metadata& metadata() const { return metadata_; }
    uint64_t decodedFrames() const { return decodedFrames_; }
    bool isOpen() const { return backend_ != nullptr; }

private:
    std::unique_ptr<Backend> backend_;
    Metadata metadata_{};
    uint64_t decodedFrames_ = 0;
};

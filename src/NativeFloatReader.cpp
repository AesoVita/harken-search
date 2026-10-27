#include "NativeFloatReader.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <utility>
#include <vector>

#include "platform/audio_data_source.h"
#include "../thirdparty/miniaudio.h"

namespace {

class MiniaudioBackend final : public NativeFloatReader::Backend {
public:
    ~MiniaudioBackend() override { close(); }

    bool open(const std::string& path) {
        if (!Platform::AudioDataSource::InitializeDecoderFromFile(decoder_, path).Succeeded())
            return false;
        open_ = true;
        const auto format = Platform::AudioDataSource::QueryDataFormat(
            Platform::AudioDataSource::BorrowDataSource(decoder_));
        metadata_.channels = format.channels;
        metadata_.sampleRate = format.sampleRate;
        const auto lengthResult = Platform::AudioDataSource::QueryLength(
            Platform::AudioDataSource::BorrowDataSource(decoder_));
        if (lengthResult.Succeeded() && lengthResult.frames > 0) {
            metadata_.reportedFrames = lengthResult.frames;
            metadata_.hasReportedFrames = true;
        }
        if (!(metadata_.channels > 0 && metadata_.sampleRate > 0)) {
            close();
            return false;
        }
        return true;
    }

    NativeFloatReader::Metadata probe() const override { return metadata_; }

    NativeFloatReader::ReadResult read(float* dst, uint64_t capacityFrames) override {
        if (!dst || capacityFrames == 0)
            return {NativeFloatReader::ReadStatus::Error, 0, "invalid_read_buffer"};
        const auto readResult = Platform::AudioDataSource::ReadPcmFrames(
            Platform::AudioDataSource::BorrowDataSource(decoder_),
            dst,
            capacityFrames);
        const int result = readResult.backendCode;
        const uint64_t frames = readResult.framesRead;
        NativeFloatReader::ReadStatus status = NativeFloatReader::ReadStatus::Frames;
        std::string error;
        if (frames == 0) {
            if (result == MA_AT_END) status = NativeFloatReader::ReadStatus::End;
            else if (result != MA_SUCCESS) {
                status = NativeFloatReader::ReadStatus::Error;
                error = "decoder_read_failed";
            } else {
                status = NativeFloatReader::ReadStatus::End;
            }
        } else if (result != MA_SUCCESS && result != MA_AT_END) {
            status = NativeFloatReader::ReadStatus::Error;
            error = "decoder_read_partial_error";
        }
        return {status, frames, std::move(error)};
    }

    NativeFloatReader::ReadResult seek(uint64_t frame) override {
        if (!Platform::AudioDataSource::SeekPcmFrame(
                Platform::AudioDataSource::BorrowDataSource(decoder_),
                frame).Succeeded())
            return {NativeFloatReader::ReadStatus::Error, 0, "decoder_seek_failed"};
        return {NativeFloatReader::ReadStatus::Frames, 0, {}};
    }

    void close() override {
        if (open_) {
            Platform::AudioDataSource::UninitializeDecoder(decoder_);
            open_ = false;
        }
    }

private:
    Platform::AudioDataSource::Decoder decoder_;
    NativeFloatReader::Metadata metadata_{};
    bool open_ = false;
};

} // namespace

NativeFloatReader::NativeFloatReader(std::unique_ptr<Backend> backend)
    : backend_(std::move(backend)) {
    if (backend_) metadata_ = backend_->probe();
}

NativeFloatReader::~NativeFloatReader() { close(); }

NativeFloatReader NativeFloatReader::ForTest(std::unique_ptr<Backend> backend) {
    return NativeFloatReader(std::move(backend));
}

bool NativeFloatReader::IsPathSupported(const std::string& path) {
    const size_t dot = path.find_last_of('.');
    if (dot == std::string::npos || dot + 1 >= path.size()) return false;
    std::string ext = path.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    return ext == ".wav" || ext == ".wave" || ext == ".mp3" || ext == ".ogg" ||
           ext == ".oga" || ext == ".flac" || ext == ".aif" || ext == ".aiff";
}

bool NativeFloatReader::open(const std::string& path) {
    close();
    auto backend = std::make_unique<MiniaudioBackend>();
    if (!backend->open(path)) return false;
    backend_ = std::move(backend);
    metadata_ = backend_->probe();
    decodedFrames_ = 0;
    return true;
}

NativeFloatReader::ReadResult NativeFloatReader::read(
    float* interleaved, uint64_t capacityFrames, const std::atomic<bool>* cancelFlag) {
    if (cancelFlag && cancelFlag->load(std::memory_order_acquire))
        return {ReadStatus::Cancelled, 0, "cancelled"};
    if (!backend_) return {ReadStatus::Error, 0, "reader_not_open"};
    if (metadata_.channels == 0 || capacityFrames >
        (uint64_t)std::numeric_limits<size_t>::max() / metadata_.channels)
        return {ReadStatus::Error, 0, "invalid_read_buffer"};
    ReadResult result = backend_->read(interleaved, capacityFrames);
    decodedFrames_ += result.framesRead;
    return result;
}

NativeFloatReader::ReadResult NativeFloatReader::seek(
    uint64_t frame, const std::atomic<bool>* cancelFlag) {
    if (cancelFlag && cancelFlag->load(std::memory_order_acquire))
        return {ReadStatus::Cancelled, 0, "cancelled"};
    if (!backend_) return {ReadStatus::Error, 0, "reader_not_open"};
    return backend_->seek(frame);
}

void NativeFloatReader::close() {
    if (backend_) backend_->close();
    backend_.reset();
    metadata_ = {};
    decodedFrames_ = 0;
}

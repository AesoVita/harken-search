#include "audio_file_reader_scripted.h"
#include <algorithm>
#include <atomic>
#include <memory>

namespace AudioFileScripted {
namespace { State state; }
State& Get() { return state; }
void Reset() { state = {}; }
void QueueOpen(Status status) { state.openQueue.push_back(status); }
void QueueSeek(Status status) { state.seekQueue.push_back(status); }
void QueueRead(ReadResult result, std::vector<float> payload) {
    state.readQueue.push_back({result, std::move(payload)});
}
} // namespace AudioFileScripted

namespace Platform::AudioFile {
struct MonoFloatReader::Impl {};
}

namespace Platform::AudioFile {
static AudioFileScripted::State& ScriptState() { return AudioFileScripted::Get(); }

MonoFloatReader::MonoFloatReader() noexcept = default;
MonoFloatReader::~MonoFloatReader() { Close(); }
MonoFloatReader::MonoFloatReader(MonoFloatReader&&) noexcept = default;
MonoFloatReader& MonoFloatReader::operator=(MonoFloatReader&& other) noexcept {
    if (this != &other) {
        Close();
        impl_ = std::move(other.impl_);
    }
    return *this;
}

Status MonoFloatReader::Open(const std::string& path, LegacyWindowsPathPolicy policy,
                             MonoSampleRate sampleRate) {
    Close();
    auto& s = ScriptState();
    s.calls.push_back("Open"); s.paths.push_back(path); s.policies.push_back(policy);
    s.sampleRates.push_back(sampleRate);
    const auto status = s.openIndex < s.openQueue.size() ? s.openQueue[s.openIndex++] : Status::OpenFailed;
    if (status != Status::Ok) return status;
    impl_ = std::make_unique<Impl>();
    return Status::Ok;
}
ReadResult MonoFloatReader::Read(float* destination, FrameCount requested,
                                 const std::atomic<bool>* cancel) {
    if (!impl_ || !destination || requested == 0) {
        if (requested == 0) return {Status::Ok, 0};
        return {Status::InvalidArgument, 0};
    }
    if (cancel && cancel->load(std::memory_order_acquire)) return {Status::Cancelled, 0};
    auto& s = ScriptState(); s.calls.push_back("Read"); s.readRequests.push_back(requested);
    if (s.readIndex >= s.readQueue.size()) return {Status::EndOfFile, 0};
    const auto& step = s.readQueue[s.readIndex++];
    const auto count = std::min({requested, step.result.framesRead,
                                 static_cast<FrameCount>(step.payload.size())});
    std::copy_n(step.payload.begin(), static_cast<std::size_t>(count), destination);
    return {step.result.status, count};
}
Status MonoFloatReader::Seek(FrameCount frame) {
    if (!impl_) return Status::SeekFailed;
    auto& s = ScriptState(); s.calls.push_back("Seek"); s.seeks.push_back(frame);
    return s.seekIndex < s.seekQueue.size() ? s.seekQueue[s.seekIndex++] : Status::SeekFailed;
}
bool MonoFloatReader::IsOpen() const noexcept { return impl_ != nullptr; }
void MonoFloatReader::Close() noexcept {
    if (!impl_) return;
    ScriptState().calls.push_back("Close");
    impl_.reset();
}
} // namespace Platform::AudioFile

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "src/NativeFloatReader.h"

namespace {

struct ScriptedBackend final : NativeFloatReader::Backend {
    NativeFloatReader::Metadata metadata{48000, 2, 99, true};
    std::vector<float> samples{0.1f, -0.1f, 0.2f, -0.2f};
    NativeFloatReader::ReadResult terminal{NativeFloatReader::ReadStatus::End, 0, {}};
    uint64_t position = 0;
    int reads = 0;
    int seeks = 0;
    bool emitPartialError = false;

    NativeFloatReader::Metadata probe() const override { return metadata; }

    NativeFloatReader::ReadResult read(float* dst, uint64_t capacityFrames) override {
        ++reads;
        const uint64_t available = samples.size() / metadata.channels - position;
        const uint64_t frames = emitPartialError ? (std::min<uint64_t>)(1, available)
                                                 : (std::min)(available, capacityFrames);
        for (uint64_t i = 0; i < frames * metadata.channels; ++i)
            dst[i] = samples[position * metadata.channels + i];
        position += frames;
        if (emitPartialError) {
            emitPartialError = false;
            return {NativeFloatReader::ReadStatus::Error, frames, "codec_error"};
        }
        if (frames > 0) return {NativeFloatReader::ReadStatus::Frames, frames, {}};
        return terminal;
    }

    NativeFloatReader::ReadResult seek(uint64_t frame) override {
        ++seeks;
        position = frame;
        return {NativeFloatReader::ReadStatus::Frames, 0, {}};
    }

    void close() override {}
};

bool Check(bool condition, const char* label, int& failures) {
    if (!condition) {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", label);
    }
    return condition;
}

} // namespace

int main() {
    int failures = 0;

    auto backend = std::make_unique<ScriptedBackend>();
    ScriptedBackend* raw = backend.get();
    NativeFloatReader reader = NativeFloatReader::ForTest(std::move(backend));
    Check(reader.metadata().channels == 2 && reader.metadata().sampleRate == 48000,
          "native metadata preserves channels and sample rate", failures);
    Check(reader.metadata().hasReportedFrames && reader.metadata().reportedFrames == 99,
          "reported length is exposed as a hint", failures);

    float buffer[4] = {};
    const NativeFloatReader::ReadResult first = reader.read(buffer, 2);
    Check(first.status == NativeFloatReader::ReadStatus::Frames && first.framesRead == 2,
          "interleaved read reports frame count", failures);
    Check(buffer[0] == 0.1f && buffer[1] == -0.1f && buffer[2] == 0.2f,
          "interleaved samples retain source order", failures);
    Check(reader.decodedFrames() == 2, "decoded frame accounting uses frames, not floats", failures);

    Check(reader.seek(0).status == NativeFloatReader::ReadStatus::Frames && raw->seeks == 1,
          "seek delegates to backend", failures);
    Check(reader.read(buffer, 1).framesRead == 1 && reader.decodedFrames() == 3,
          "seek does not corrupt cumulative progress accounting", failures);

    std::atomic<bool> cancel{true};
    auto cancelledBackend = std::make_unique<ScriptedBackend>();
    NativeFloatReader cancelled = NativeFloatReader::ForTest(std::move(cancelledBackend));
    Check(cancelled.read(buffer, 1, &cancel).status == NativeFloatReader::ReadStatus::Cancelled,
          "cancel-before-read returns cancelled", failures);
    Check(cancelled.decodedFrames() == 0, "cancel-before-read consumes no frames", failures);

    auto partialBackend = std::make_unique<ScriptedBackend>();
    partialBackend->emitPartialError = true;
    NativeFloatReader partial = NativeFloatReader::ForTest(std::move(partialBackend));
    const auto partialResult = partial.read(buffer, 2);
    Check(partialResult.status == NativeFloatReader::ReadStatus::Error &&
              partialResult.framesRead == 1 && partialResult.error == "codec_error",
          "partial frames are returned with terminal error", failures);
    Check(partial.decodedFrames() == 1, "partial error preserves progress", failures);

    Check(NativeFloatReader::IsPathSupported("clip.wav"), "WAV path is supported", failures);
    Check(!NativeFloatReader::IsPathSupported("clip.txt"), "unsupported extension is rejected", failures);

    return failures == 0 ? 0 : 1;
}

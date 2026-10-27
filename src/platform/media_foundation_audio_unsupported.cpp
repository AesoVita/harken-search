#include "../MediaFoundationAudioDecoder.h"

namespace HarkenSearch::MediaFoundationAudio {

bool Probe(const std::string& path,
           uint32_t& sampleRate,
           uint32_t& channels,
           uint64_t& totalFrames,
           float& durationSeconds,
           std::string* error) {
    sampleRate = 0;
    channels = 0;
    totalFrames = 0;
    durationSeconds = 0.0f;
    if (!CanDecodeExtension(path)) {
        if (error) *error = "media_foundation_extension_not_supported";
        return false;
    }
    if (error) *error = "media_foundation_unavailable";
    return false;
}

bool Decode(const std::string& path,
            DecodedAudioBuffer& out,
            std::string* error,
            const std::atomic<bool>* /*cancelFlag*/) {
    out = {};
    if (!CanDecodeExtension(path)) {
        if (error) *error = "media_foundation_extension_not_supported";
        return false;
    }
    if (error) *error = "media_foundation_unavailable";
    return false;
}

} // namespace HarkenSearch::MediaFoundationAudio

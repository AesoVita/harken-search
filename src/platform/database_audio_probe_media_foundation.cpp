#include "platform/database_audio_probe_backend.h"

#include "MediaFoundationAudioDecoder.h"

namespace Platform::DatabaseAudioProbe::Detail {

bool CanDecodeExtension(const std::string& pathUtf8) {
    return HarkenSearch::MediaFoundationAudio::CanDecodeExtension(pathUtf8);
}

bool IsFlacExtension(const std::string& pathUtf8) {
    return HarkenSearch::MediaFoundationAudio::HasFlacExtension(pathUtf8);
}

bool ProbeMediaFoundation(const std::string& pathUtf8,
                          Info& out,
                          std::string& error) {
    return HarkenSearch::MediaFoundationAudio::Probe(
        pathUtf8,
        out.sampleRate,
        out.channels,
        out.totalFrames,
        out.durationSeconds,
        &error);
}

} // namespace Platform::DatabaseAudioProbe::Detail

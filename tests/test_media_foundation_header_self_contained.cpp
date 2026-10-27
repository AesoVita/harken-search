#include "MediaFoundationAudioDecoder.h"

#include <cstdio>
#include <string>
#include <vector>

namespace {

int failures = 0;

void Check(bool condition, const char* message) {
    if (!condition) {
        std::printf("FAIL: %s\n", message);
        ++failures;
    }
}

struct PeakResult {
    std::vector<float> peaks;
    std::vector<float> minPeaks;
    std::vector<float> rms;
    int channels = 0;
    int samplesPerPeak = 0;
    uint64_t decodedFrames = 0;
    uint32_t sampleRate = 0;
    float trueRmsDbfs = -100.0f;
    bool hasTrueRms = false;
    bool ok = false;
    std::string errorCode;
};

} // namespace

int main() {
    using namespace HarkenSearch::MediaFoundationAudio;

    Check(LowerExtension("C:/Audio/VOICE.WMA") == ".wma",
          "LowerExtension lowercases an uppercase extension");
    Check(LowerExtension("C:/Audio/no_extension").empty(),
          "LowerExtension keeps a path without an extension empty");
    Check(ClassifyFailure("DRM blocked") ==
              "wma_media_foundation_drm_or_protected:DRM blocked",
          "ClassifyFailure preserves DRM classification");
    Check(ClassifyFailure("codec unavailable") ==
              "wma_media_foundation_codec_missing_or_unsupported:codec unavailable",
          "ClassifyFailure preserves codec classification");

    DecodedAudioBuffer decoded;
    Check(decoded.sampleRate == 0 && decoded.channels == 0 &&
              decoded.totalFrames == 0 && decoded.durationSeconds == 0.0f &&
              decoded.interleaved.empty(),
          "DecodedAudioBuffer defaults remain empty");

    decoded.sampleRate = 48000;
    decoded.channels = 2;
    decoded.totalFrames = 2;
    decoded.interleaved = {0.5f, -0.25f, -0.75f, 1.0f};
    PeakResult peaks;
    Check(BuildPeaksFromDecoded(decoded, 2, true, true, peaks),
          "BuildPeaksFromDecoded accepts in-memory PCM");
    Check(peaks.channels == 2 && peaks.sampleRate == 48000 &&
              peaks.decodedFrames == 2 && peaks.peaks.size() == 4 &&
              peaks.peaks[0] == 0.5f && peaks.minPeaks[1] == -0.25f &&
              peaks.minPeaks[2] == -0.75f && peaks.peaks[3] == 1.0f &&
              peaks.hasTrueRms,
          "BuildPeaksFromDecoded preserves peak shape and signed extrema");

    if (failures == 0) {
        std::puts("PASS: MediaFoundationAudioDecoder header is self-contained");
    }
    return failures == 0 ? 0 : 1;
}

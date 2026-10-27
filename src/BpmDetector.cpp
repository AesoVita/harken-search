#include "BpmDetector.h"
#include "platform/audio_file.h"
#include <aubio/aubio.h>
#include <vector>
#include <cmath>
#include <algorithm>

// Find first and last 512-sample blocks whose RMS exceeds -60 dBFS (0.001).
// Returns {start, end} frame indices (end exclusive). Returns {0,0} for
// all-silence input so the caller's numFrames-check rejects it.
static std::pair<int,int> bpmTrimSilence(const float* pcm, int numFrames,
                                          int blockSize = 512,
                                          float rmsThreshold = 0.001f) {
    int start = numFrames;
    for (int i = 0; i + blockSize <= numFrames; i += blockSize) {
        float rms = 0.0f;
        for (int j = 0; j < blockSize; ++j) { float s = pcm[i + j]; rms += s * s; }
        if (std::sqrt(rms / blockSize) >= rmsThreshold) { start = i; break; }
    }
    if (start >= numFrames) return {0, 0}; // all silent

    int end = blockSize;
    for (int i = ((numFrames - 1) / blockSize) * blockSize; i >= start; i -= blockSize) {
        int bEnd = std::min(i + blockSize, numFrames);
        float rms = 0.0f;
        for (int j = i; j < bEnd; ++j) { float s = pcm[j]; rms += s * s; }
        if (std::sqrt(rms / (bEnd - i)) >= rmsThreshold) { end = bEnd; break; }
    }
    return {start, end};
}

BpmResult BpmDetector::detect(const float* pcm, int numFrames,
                               int channels, uint32_t sampleRate) {
    BpmResult result;
    if (!pcm || numFrames <= 0 || sampleRate == 0) return result;
    if (numFrames < static_cast<int>(sampleRate)) return result; // need >= 1s

    // Downmix to mono
    std::vector<float> mono(static_cast<size_t>(numFrames));
    if (channels == 1) {
        std::copy(pcm, pcm + numFrames, mono.begin());
    } else {
        for (int i = 0; i < numFrames; ++i) {
            float sum = 0.0f;
            for (int c = 0; c < channels; ++c)
                sum += pcm[i * channels + c];
            mono[static_cast<size_t>(i)] = sum / static_cast<float>(channels);
        }
    }

    // Trim leading and trailing silence so aubio only sees musical content
    auto [trimStart, trimEnd] = bpmTrimSilence(mono.data(), numFrames);
    int trimmedFrames = trimEnd - trimStart;
    if (trimmedFrames < static_cast<int>(sampleRate)) return result; // < 1s after trim
    mono.erase(mono.begin(), mono.begin() + trimStart);
    mono.resize(static_cast<size_t>(trimmedFrames));
    numFrames = trimmedFrames;

    const uint_t winSize = 1024;
    const uint_t hopSize = 512;

    aubio_tempo_t* tempo = new_aubio_tempo("default", winSize, hopSize,
                                           static_cast<uint_t>(sampleRate));
    if (!tempo) return result;

    fvec_t* inputBuf = new_fvec(hopSize);
    if (!inputBuf) { del_aubio_tempo(tempo); return result; }

    fvec_t* beatBuf = new_fvec(1);
    if (!beatBuf) { del_fvec(inputBuf); del_aubio_tempo(tempo); return result; }

    float beatOffset = -1.0f;

    for (int offset = 0; offset + static_cast<int>(hopSize) <= numFrames;
         offset += static_cast<int>(hopSize)) {
        for (uint_t i = 0; i < hopSize; ++i)
            inputBuf->data[i] = mono[static_cast<size_t>(offset) + i];

        aubio_tempo_do(tempo, inputBuf, beatBuf);

        if (beatBuf->data[0] != 0.0f && beatOffset < 0.0f)
            beatOffset = static_cast<float>(offset) / static_cast<float>(sampleRate);
    }

    float bpm  = aubio_tempo_get_bpm(tempo);
    float conf = aubio_tempo_get_confidence(tempo);

    del_fvec(inputBuf);
    del_fvec(beatBuf);
    del_aubio_tempo(tempo);

    if (bpm < 40.0f || bpm > 240.0f) return result;

    result.bpm        = bpm;
    result.confidence = std::clamp(conf, 0.0f, 1.0f);
    result.beatOffset = std::max(0.0f, beatOffset);
    result.ok         = true;
    return result;
}

BpmResult BpmDetector::detectFile(const std::string& filePath,
                                   double startSec, double endSec,
                                   double maxSec) {
    BpmResult result;

    Platform::AudioFile::MonoFloatReader reader;
    if (reader.Open(
            filePath,
            Platform::AudioFile::LegacyWindowsPathPolicy::KeyPermissiveUtf8CanonicalPrefix,
            Platform::AudioFile::MonoSampleRate::Hz44100) !=
        Platform::AudioFile::Status::Ok)
        return result;

    if (startSec > 0.0) {
        const auto startFrame = static_cast<Platform::AudioFile::FrameCount>(
            startSec * 44100.0);
        reader.Seek(startFrame);
    }

    double analysisDur = (endSec > 0.0) ? (endSec - startSec) : maxSec;
    analysisDur = std::min(analysisDur, maxSec);
    int framesToRead = static_cast<int>(analysisDur * 44100.0);

    std::vector<float> pcm(static_cast<size_t>(framesToRead));
    const auto read = reader.Read(
        pcm.data(), static_cast<Platform::AudioFile::FrameCount>(framesToRead));
    reader.Close();

    if (static_cast<int>(read.framesRead) < 44100) return result; // need >= 1s

    return detect(pcm.data(), static_cast<int>(read.framesRead), 1, 44100);
}

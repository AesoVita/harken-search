#include "KeyDetector.h"
#include "platform/audio_file.h"
#include <complex>
#include <cmath>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cstring>
// ---- Constants ---------------------------------------------------------------

static constexpr int   kKDSampleRate = 22050;
static constexpr int   kKDFftSize    = 4096;
static constexpr int   kKDHopSize    = 1024;
static constexpr float kKDPi         = 3.14159265358979f;

// ---- Key tables (indexed by pitch class 0=C, 1=C#, 2=D, ..., 11=B) ----------

static const char* kMajorNames[12]   = {"C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B"};
static const char* kMinorNames[12]   = {"Cm","C#m","Dm","Ebm","Em","Fm","F#m","Gm","Abm","Am","Bbm","Bm"};
static const char* kMajorCamelot[12] = {"8B","3B","10B","5B","12B","7B","2B","9B","4B","11B","6B","1B"};
static const char* kMinorCamelot[12] = {"5A","12A","7A","2A","9A","4A","11A","6A","1A","8A","3A","10A"};

// Krumhansl-Schmuckler 1990 key profiles
static const float kMajorProf[12] = {6.35f,2.23f,3.48f,2.33f,4.38f,4.09f,2.52f,5.19f,2.39f,3.66f,2.29f,2.88f};
static const float kMinorProf[12] = {6.33f,2.68f,3.52f,5.38f,2.60f,3.53f,2.54f,4.75f,3.98f,2.69f,3.34f,3.17f};

// ---- Radix-2 FFT (Cooley-Tukey) ----------------------------------------------

static void kdFFT(std::vector<std::complex<float>>& x) {
    size_t N = x.size();
    if (N <= 1) return;
    for (size_t i = 1, j = 0; i < N; i++) {
        size_t bit = N >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(x[i], x[j]);
    }
    for (size_t len = 2; len <= N; len <<= 1) {
        float angle = -2.0f * kKDPi / (float)len;
        std::complex<float> wlen(std::cos(angle), std::sin(angle));
        for (size_t i = 0; i < N; i += len) {
            std::complex<float> w(1.0f, 0.0f);
            for (size_t j2 = 0; j2 < len / 2; j2++) {
                auto u = x[i + j2];
                auto t = w * x[i + j2 + len / 2];
                x[i + j2]           = u + t;
                x[i + j2 + len / 2] = u - t;
                w *= wlen;
            }
        }
    }
}

// ---- Hann window -------------------------------------------------------------

static void kdHann(std::vector<float>& win, int N) {
    win.resize(N);
    for (int i = 0; i < N; ++i)
        win[i] = 0.5f * (1.0f - std::cos(2.0f * kKDPi * i / (N - 1)));
}

// ---- Pearson correlation between chroma and a key profile --------------------

static float kdPearson(const float* a, const float* b, int n) {
    float ma = 0, mb = 0;
    for (int i = 0; i < n; ++i) { ma += a[i]; mb += b[i]; }
    ma /= n; mb /= n;
    float num = 0, da = 0, db = 0;
    for (int i = 0; i < n; ++i) {
        float ea = a[i] - ma, eb = b[i] - mb;
        num += ea * eb;
        da  += ea * ea;
        db  += eb * eb;
    }
    float denom = std::sqrt(da * db);
    return denom < 1e-12f ? 0.0f : num / denom;
}

// ---- Core detection ----------------------------------------------------------
KeyResult KeyDetector::detect(const float* pcm, int numFrames,
                               int channels, uint32_t sampleRate) {
    KeyResult result;
    if (!pcm) return result;
    if (!pcm || numFrames < static_cast<int>(sampleRate)) return result;

    // Downmix to mono
    std::vector<float> mono(static_cast<size_t>(numFrames));
    if (channels == 1) {
        std::copy(pcm, pcm + numFrames, mono.begin());
    } else {
        for (int i = 0; i < numFrames; ++i) {
            float s = 0.0f;
            for (int c = 0; c < channels; ++c) s += pcm[i * channels + c];
            mono[static_cast<size_t>(i)] = s / channels;
        }
    }

    // Resample to kKDSampleRate if needed (simple linear interpolation)
    std::vector<float> sig;
    if (sampleRate != kKDSampleRate) {
        double ratio = static_cast<double>(kKDSampleRate) / sampleRate;
        int outLen = static_cast<int>(numFrames * ratio);
        sig.resize(outLen);
        for (int i = 0; i < outLen; ++i) {
            double src = i / ratio;
            int s0 = static_cast<int>(src);
            int s1 = std::min(s0 + 1, numFrames - 1);
            float t = static_cast<float>(src - s0);
            sig[static_cast<size_t>(i)] = mono[static_cast<size_t>(s0)] * (1.0f - t) +
                                          mono[static_cast<size_t>(s1)] * t;
        }
    } else {
        sig = mono;
    }

    int sigLen = static_cast<int>(sig.size());
    if (sigLen < kKDFftSize) return result;

    // Build Hann window
    std::vector<float> hann;
    kdHann(hann, kKDFftSize);

    // Precompute freq->pitch class mapping for FFT bins
    // bin b -> freq = b * kKDSampleRate / kKDFftSize
    // MIDI = 69 + 12*log2(freq/440) => pc = (round(MIDI) % 12 + 12) % 12
    // valid range: 27.5 Hz (A0) to 4186 Hz (C8)
    std::vector<int> binToPc(kKDFftSize / 2 + 1, -1);
    for (int b = 1; b <= kKDFftSize / 2; ++b) {
        float freq = static_cast<float>(b) * kKDSampleRate / kKDFftSize;
        if (freq < 27.5f || freq > 4186.0f) continue;
        float midi = 69.0f + 12.0f * std::log2(freq / 440.0f);
        int pc = (static_cast<int>(std::round(midi)) % 12 + 12) % 12;
        binToPc[static_cast<size_t>(b)] = pc;
    }

    // Accumulate chromagram across frames
    float chroma[12] = {};
    int frameCount = 0;
    std::vector<std::complex<float>> fftBuf(static_cast<size_t>(kKDFftSize));

    for (int offset = 0; offset + kKDFftSize <= sigLen; offset += kKDHopSize) {
        for (int i = 0; i < kKDFftSize; ++i)
            fftBuf[static_cast<size_t>(i)] = {sig[static_cast<size_t>(offset + i)] * hann[static_cast<size_t>(i)], 0.0f};
        kdFFT(fftBuf);
        for (int b = 1; b <= kKDFftSize / 2; ++b) {
            int pc = binToPc[static_cast<size_t>(b)];
            if (pc < 0) continue;
            float mag = std::abs(fftBuf[static_cast<size_t>(b)]);
            chroma[pc] += mag * mag;  // accumulate power
        }
        ++frameCount;
    }
    if (frameCount == 0) return result;

    // Normalize chroma
    float chromaMax = *std::max_element(chroma, chroma + 12);
    if (chromaMax < 1e-20f) return result;
    for (int i = 0; i < 12; ++i) chroma[i] /= chromaMax;

    // Match against all 24 key profiles (12 major + 12 minor)
    float bestR  = -2.0f;
    float best2R = -2.0f;
    int   bestKey  = 0;
    bool  bestMinor = false;

    for (int root = 0; root < 12; ++root) {
        // Build shifted profile
        float maj[12], min[12];
        for (int i = 0; i < 12; ++i) {
            maj[i] = kMajorProf[(i - root + 12) % 12];
            min[i] = kMinorProf[(i - root + 12) % 12];
        }
        float rMaj = kdPearson(chroma, maj, 12);
        float rMin = kdPearson(chroma, min, 12);

        auto update = [&](float r, bool isMin, int root2) {
            if (r > bestR) {
                best2R = bestR;
                bestR = r; bestKey = root2; bestMinor = isMin;
            } else if (r > best2R) {
                best2R = r;
            }
        };
        update(rMaj, false, root);
        update(rMin, true,  root);
    }

    if (bestR < 0.1f) return result;  // no tonal content

    // Confidence: how clearly the best key stands out from the second best
    float gap = bestR - best2R;
    result.confidence = std::clamp(gap * 4.0f, 0.0f, 1.0f);

    result.key     = bestMinor ? kMinorNames[bestKey] : kMajorNames[bestKey];
    result.camelot = bestMinor ? kMinorCamelot[bestKey] : kMajorCamelot[bestKey];
    result.isMinor = bestMinor;
    result.ok      = true;
    return result;
}

KeyResult KeyDetector::detectFile(const std::string& filePath,
                                   double startSec, double endSec,
                                   double maxSec) {
    KeyResult result;
    Platform::AudioFile::MonoFloatReader reader;
    if (reader.Open(filePath, Platform::AudioFile::LegacyWindowsPathPolicy::KeyPermissiveUtf8CanonicalPrefix) !=
        Platform::AudioFile::Status::Ok) return result;

    if (startSec > 0.0)
        reader.Seek(static_cast<Platform::AudioFile::FrameCount>(startSec * kKDSampleRate));

    double dur = (endSec > 0.0) ? (endSec - startSec) : maxSec;
    dur = std::min(dur, maxSec);
    int frames = static_cast<int>(dur * kKDSampleRate);

    std::vector<float> pcm(static_cast<size_t>(frames));
    const auto readResult = reader.Read(pcm.data(), static_cast<Platform::AudioFile::FrameCount>(frames));
    const auto read = readResult.framesRead;

    if (static_cast<int>(read) < kKDSampleRate) return result;
    return detect(pcm.data(), static_cast<int>(read), 1, kKDSampleRate);
}

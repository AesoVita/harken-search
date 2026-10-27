#pragma once

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace HarkenSearch::MediaFoundationAudio {

// @stage7-contract
// API contract: decoder helpers accept UTF-8 paths and return owned decoded
// buffers. Output metadata describes the interleaved sample vector exactly;
// failed or cancelled operations return their established empty/false result
// without exposing backend handles or partial ownership.

struct DecodedAudioBuffer {
    uint32_t sampleRate = 0;
    uint32_t channels = 0;
    uint64_t totalFrames = 0;
    float durationSeconds = 0.0f;
    std::vector<float> interleaved;
};

inline std::string LowerExtension(const std::string& path) {
    const size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) return {};
    std::string ext = path.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return (char)std::tolower(c);
    });
    return ext;
}

inline bool HasWmaExtension(const std::string& path) {
    return LowerExtension(path) == ".wma";
}

inline bool HasFlacExtension(const std::string& path) {
    return LowerExtension(path) == ".flac";
}

inline bool CanDecodeExtension(const std::string& path) {
    const std::string ext = LowerExtension(path);
    return ext == ".wma" ||
           ext == ".flac" ||
           ext == ".m4a" ||
           ext == ".aac" ||
           ext == ".mp4" ||
           ext == ".m4p";
}

inline const char* FormatNameFromPath(const std::string& path) {
    const std::string ext = LowerExtension(path);
    if (ext == ".wma") return "WMA";
    if (ext == ".flac") return "FLAC";
    if (ext == ".m4a") return "M4A";
    if (ext == ".aac") return "AAC";
    if (ext == ".mp4") return "MP4";
    if (ext == ".m4p") return "M4P";
    return "MediaFoundation";
}

inline std::string ClassifyFailure(const std::string& detail) {
    if (detail.empty()) return "wma_media_foundation_unsupported";
    if (detail.rfind("wma_media_foundation_", 0) == 0) return detail;
    std::string lower = detail;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return (char)std::tolower(c);
    });
    if (lower.find("drm") != std::string::npos ||
        lower.find("0xc00d271d") != std::string::npos ||
        lower.find("0xc00d2728") != std::string::npos) {
        return "wma_media_foundation_drm_or_protected:" + detail;
    }
    if (lower.find("codec") != std::string::npos ||
        lower.find("unsupported") != std::string::npos ||
        lower.find("0xc00d36b4") != std::string::npos ||
        lower.find("0xc00d36c4") != std::string::npos) {
        return "wma_media_foundation_codec_missing_or_unsupported:" + detail;
    }
    return "wma_media_foundation_unsupported:" + detail;
}

#ifdef _WIN32
namespace detail {

inline std::wstring ToMediaFoundationPath(const std::wstring& path) {
    if (path.empty()) return path;
    std::wstring normalized = path;
    std::replace(normalized.begin(), normalized.end(), L'/', L'\\');
    if (normalized.rfind(L"\\\\?\\UNC\\", 0) == 0) {
        return L"\\\\" + normalized.substr(8);
    }
    if (normalized.rfind(L"\\\\?\\", 0) == 0) {
        return normalized.substr(4);
    }
    return normalized;
}

} // namespace detail
#endif

bool Probe(const std::string& path,
           uint32_t& sampleRate,
           uint32_t& channels,
           uint64_t& totalFrames,
           float& durationSeconds,
           std::string* error = nullptr);

bool Decode(const std::string& path,
            DecodedAudioBuffer& out,
            std::string* error = nullptr,
            const std::atomic<bool>* cancelFlag = nullptr);

template <typename ResultLike>
inline bool BuildPeaksFromDecoded(const DecodedAudioBuffer& decoded,
                                  int targetPeaks,
                                  bool withRms,
                                  bool perChannel,
                                  ResultLike& out,
                                  const std::atomic<bool>* cancelFlag = nullptr) {
    out = {};
    if (targetPeaks <= 0 || decoded.channels == 0 || decoded.totalFrames == 0 ||
        decoded.interleaved.empty()) {
        out.errorCode = "wma_decode_empty";
        return false;
    }
    const int sourceChannels = (int)decoded.channels;
    const int outputChannels = perChannel ? sourceChannels : 1;
    const uint64_t frames = decoded.totalFrames;
    const int effectivePeaks = (int)std::min<uint64_t>((uint64_t)targetPeaks, frames);
    const int samplesPerPeak = std::max<int>(1, (int)(frames / (uint64_t)effectivePeaks));

    out.channels = outputChannels;
    out.sampleRate = decoded.sampleRate;
    out.samplesPerPeak = samplesPerPeak;
    out.decodedFrames = frames;
    out.peaks.assign((size_t)effectivePeaks * (size_t)outputChannels, 0.0f);
    out.minPeaks.assign((size_t)effectivePeaks * (size_t)outputChannels, 0.0f);
    if (withRms) out.rms.assign((size_t)effectivePeaks * (size_t)outputChannels, 0.0f);
    else out.rms.clear();

    double totalSumSq = 0.0;
    uint64_t totalSamples = 0;
    for (int bucket = 0; bucket < effectivePeaks; ++bucket) {
        if (cancelFlag && cancelFlag->load(std::memory_order_acquire)) {
            out = {};
            out.errorCode = "cancelled";
            return false;
        }
        uint64_t begin = ((uint64_t)bucket * frames) / (uint64_t)effectivePeaks;
        uint64_t end = (((uint64_t)bucket + 1u) * frames) / (uint64_t)effectivePeaks;
        if (end <= begin) end = begin + 1;
        end = std::min<uint64_t>(end, frames);
        const uint64_t bucketFrames = std::max<uint64_t>(1, end - begin);
        for (int outCh = 0; outCh < outputChannels; ++outCh) {
            float maxPeak = 0.0f;
            float minPeak = 0.0f;
            double sumSq = 0.0;
            for (uint64_t frame = begin; frame < end; ++frame) {
                if (perChannel) {
                    const float s = decoded.interleaved[(size_t)frame * (size_t)sourceChannels + (size_t)outCh];
                    maxPeak = std::max(maxPeak, s);
                    minPeak = std::min(minPeak, s);
                    sumSq += (double)s * (double)s;
                } else {
                    float frameMax = 0.0f;
                    float frameMin = 0.0f;
                    double frameSum = 0.0;
                    for (int ch = 0; ch < sourceChannels; ++ch) {
                        const float s = decoded.interleaved[(size_t)frame * (size_t)sourceChannels + (size_t)ch];
                        frameMax = std::max(frameMax, s);
                        frameMin = std::min(frameMin, s);
                        frameSum += (double)s * (double)s;
                    }
                    maxPeak = std::max(maxPeak, frameMax);
                    minPeak = std::min(minPeak, frameMin);
                    sumSq += frameSum / (double)sourceChannels;
                }
            }
            const size_t dst = (size_t)bucket * (size_t)outputChannels + (size_t)outCh;
            out.peaks[dst] = maxPeak;
            out.minPeaks[dst] = minPeak;
            if (withRms) {
                out.rms[dst] = (float)std::sqrt(sumSq / (double)bucketFrames);
            }
            totalSumSq += sumSq;
            totalSamples += bucketFrames;
        }
    }

    if (withRms && totalSamples > 0) {
        const double rmsLinear = std::sqrt(totalSumSq / (double)totalSamples);
        out.trueRmsDbfs = (rmsLinear > 1e-10)
            ? std::clamp((float)(20.0 * std::log10(rmsLinear)), -100.0f, 0.0f)
            : -100.0f;
        out.hasTrueRms = true;
    }
    out.ok = !out.peaks.empty();
    if (!out.ok) out.errorCode = "wma_decode_empty";
    return out.ok;
}

} // namespace HarkenSearch::MediaFoundationAudio

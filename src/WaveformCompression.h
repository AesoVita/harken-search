#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4334)
#endif
#include "../thirdparty/rlottie/src/lottie/zip/miniz.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace WaveformCompression {

// @stage7-contract
// API contract: compression helpers synchronously borrow input buffers and
// replace caller-owned output vectors. Outputs are cleared on invalid data or
// codec failure; successful vectors own all bytes/peaks. The fixed magic/header
// layout is persisted data and is not changed by Stage 7.

static constexpr uint8_t kMagic[4] = {'Z', 'W', 'A', 'V'};
static constexpr int kHeaderSize = 8;
static constexpr size_t kCompressionThresholdBytes = 8192;

inline bool ShouldCompressRawBytes(size_t rawBytes) {
    return rawBytes > kCompressionThresholdBytes;
}

inline bool IsCompressed(const void* data, int size) {
    if (!data || size < kHeaderSize) return false;
    return std::memcmp(data, kMagic, 4) == 0;
}

inline bool CompressWaveform(const float* peaks,
                             int peakCount,
                             std::vector<uint8_t>& outCompressed) {
    outCompressed.clear();
    if (!peaks || peakCount <= 0) return false;

    const size_t rawSize = static_cast<size_t>(peakCount) * sizeof(float);
    const mz_ulong bound = compressBound(static_cast<mz_ulong>(rawSize));

    outCompressed.resize(static_cast<size_t>(kHeaderSize) + static_cast<size_t>(bound));
    std::memcpy(outCompressed.data(), kMagic, 4);

    const uint32_t rawSize32 = static_cast<uint32_t>(rawSize);
    std::memcpy(outCompressed.data() + 4, &rawSize32, sizeof(rawSize32));

    mz_ulong destLen = bound;
    const int rc = compress2(outCompressed.data() + kHeaderSize,
                             &destLen,
                             reinterpret_cast<const unsigned char*>(peaks),
                             static_cast<mz_ulong>(rawSize),
                             MZ_DEFAULT_LEVEL);
    if (rc != Z_OK) {
        outCompressed.clear();
        return false;
    }

    outCompressed.resize(static_cast<size_t>(kHeaderSize) + static_cast<size_t>(destLen));
    return true;
}

inline bool DecompressWaveform(const void* compressedData,
                               int compressedSize,
                               std::vector<float>& outPeaks) {
    outPeaks.clear();
    if (!IsCompressed(compressedData, compressedSize)) return false;

    const uint8_t* src = static_cast<const uint8_t*>(compressedData);
    uint32_t rawSize32 = 0;
    std::memcpy(&rawSize32, src + 4, sizeof(rawSize32));
    if (rawSize32 == 0 || (rawSize32 % sizeof(float)) != 0) return false;

    outPeaks.resize(rawSize32 / sizeof(float));
    mz_ulong destLen = rawSize32;
    const int rc = uncompress(reinterpret_cast<unsigned char*>(outPeaks.data()),
                              &destLen,
                              src + kHeaderSize,
                              static_cast<mz_ulong>(compressedSize - kHeaderSize));
    if (rc != Z_OK || destLen != rawSize32) {
        outPeaks.clear();
        return false;
    }

    return true;
}

}  // namespace WaveformCompression

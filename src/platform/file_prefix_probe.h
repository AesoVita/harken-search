#pragma once

#ifdef _WIN32

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace Platform::FileSystem::Internal {

// @stage7-contract
// API contract: ProbeFilePrefix synchronously reads at most the fixed prefix
// from the caller-owned UTF-8 path and returns an owned diagnostic snapshot.
// It performs no mutation or retained IO; callers must inspect opened,
// sizeAvailable, and readSucceeded before using prefix/bytesRead. Native
// errors remain diagnostic values and the function is Windows-only.

struct FilePrefixProbeResult {
    bool opened = false;
    bool sizeAvailable = false;
    uint64_t sizeBytes = 0;
    bool readSucceeded = false;
    size_t bytesRead = 0;
    std::array<unsigned char, 16> prefix{};
    uint32_t nativeOpenError = 0;
};

FilePrefixProbeResult ProbeFilePrefix(const std::string& pathUtf8);

} // namespace Platform::FileSystem::Internal

#endif // _WIN32

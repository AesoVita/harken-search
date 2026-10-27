#pragma once

#ifndef AURO_FILESYSTEM_FILE_PREFIX_PROBE_TESTING
#error "File-prefix probe hooks are test-only"
#endif

#include <array>
#include <cstddef>
#include <cstdint>

namespace Platform::FileSystem::Testing {

struct FilePrefixProbeScript {
    bool openResult = false;
    uint32_t nativeOpenError = 0;
    bool sizeResult = false;
    uint64_t sizeBytes = 0;
    bool readResult = false;
    size_t bytesRead = 0;
    std::array<unsigned char, 16> prefix{};
    int openCalls = 0;
    int sizeCalls = 0;
    int readCalls = 0;
    int closeCalls = 0;
};

void SetFilePrefixProbeScript(FilePrefixProbeScript* script);

} // namespace Platform::FileSystem::Testing

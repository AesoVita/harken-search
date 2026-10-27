#include <cstdio>

#if __has_include("platform/file_prefix_probe.h") && \
    __has_include("platform/file_prefix_probe_test_hooks.h")
#define AURO_HAS_FILE_PREFIX_PROBE 1
#include "platform/file_prefix_probe.h"
#include "platform/file_prefix_probe_test_hooks.h"

#include <algorithm>
#include <array>

namespace {

using Platform::FileSystem::Internal::FilePrefixProbeResult;
using Platform::FileSystem::Internal::ProbeFilePrefix;
using Platform::FileSystem::Testing::FilePrefixProbeScript;
using Platform::FileSystem::Testing::SetFilePrefixProbeScript;

bool allZero(const FilePrefixProbeResult& result) {
    return std::all_of(result.prefix.begin(), result.prefix.end(),
                       [](unsigned char byte) { return byte == 0; });
}

void resetScript(FilePrefixProbeScript& script) {
    SetFilePrefixProbeScript(nullptr);
    script = {};
}

} // namespace
#endif

int main() {
#ifndef AURO_HAS_FILE_PREFIX_PROBE
    std::fprintf(stderr, "FAIL: Windows file-prefix probe PAL contract is missing\n");
    return 1;
#else
    int failures = 0;
    const auto check = [&](bool condition, const char* message) {
        if (!condition) {
            ++failures;
            std::fprintf(stderr, "FAIL: %s\n", message);
        }
    };

    FilePrefixProbeScript script;
    script.openResult = false;
    script.nativeOpenError = 1234;
    SetFilePrefixProbeScript(&script);
    auto result = ProbeFilePrefix("missing.wav");
    check(!result.opened && result.nativeOpenError == 1234,
          "open failure preserves the native error");
    check(!result.sizeAvailable && result.sizeBytes == 0 &&
              !result.readSucceeded && result.bytesRead == 0 && allZero(result),
          "open failure returns deterministic empty diagnostics");
    check(script.openCalls == 1 && script.sizeCalls == 0 &&
              script.readCalls == 0 && script.closeCalls == 0,
          "open failure performs no operations on an invalid handle");

    resetScript(script);
    script.openResult = true;
    script.sizeResult = false;
    script.readResult = true;
    script.bytesRead = 4;
    script.prefix = {0x52, 0x49, 0x46, 0x46};
    SetFilePrefixProbeScript(&script);
    result = ProbeFilePrefix("size-failure.wav");
    check(result.opened && result.nativeOpenError == 0 &&
              !result.sizeAvailable && result.sizeBytes == 0,
          "size failure is deterministic after a successful open");
    check(result.readSucceeded && result.bytesRead == 4 &&
              result.prefix[0] == 0x52 && result.prefix[1] == 0x49 &&
              result.prefix[2] == 0x46 && result.prefix[3] == 0x46 &&
              result.prefix[4] == 0,
          "size failure does not discard a successful partial prefix");
    check(script.openCalls == 1 && script.sizeCalls == 1 &&
              script.readCalls == 1 && script.closeCalls == 1,
          "size failure still reads and closes exactly once");

    resetScript(script);
    script.openResult = true;
    script.sizeResult = true;
    script.sizeBytes = 4096;
    script.readResult = false;
    script.bytesRead = 0;
    SetFilePrefixProbeScript(&script);
    result = ProbeFilePrefix("read-failure.wav");
    check(result.opened && result.sizeAvailable && result.sizeBytes == 4096,
          "read failure preserves an available file size");
    check(!result.readSucceeded && result.bytesRead == 0 && allZero(result),
          "read failure preserves zero-filled diagnostic bytes");
    check(script.closeCalls == 1,
          "read failure closes the opened handle exactly once");

    resetScript(script);
    script.openResult = true;
    script.sizeResult = true;
    script.sizeBytes = 3;
    script.readResult = true;
    script.bytesRead = 3;
    script.prefix = {0x01, 0x80, 0xFF};
    SetFilePrefixProbeScript(&script);
    result = ProbeFilePrefix("partial.wav");
    check(result.opened && result.sizeAvailable && result.sizeBytes == 3 &&
              result.readSucceeded && result.bytesRead == 3,
          "partial read reports its exact successful byte count");
    check(result.prefix[0] == 0x01 && result.prefix[1] == 0x80 &&
              result.prefix[2] == 0xFF && result.prefix[3] == 0 &&
              result.prefix[15] == 0,
          "partial read retains its bytes and zero-fills the unread suffix");
    check(script.openCalls == 1 && script.sizeCalls == 1 &&
              script.readCalls == 1 && script.closeCalls == 1,
          "partial read closes the opened handle exactly once");

    SetFilePrefixProbeScript(nullptr);
    return failures == 0 ? 0 : 1;
#endif
}

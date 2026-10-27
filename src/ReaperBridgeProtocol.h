#pragma once

#include "ReaperBridgeVersion.h"

#include <charconv>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

// @stage7-contract
// API contract: ReaperBridge protocol helpers are pure synchronous encoders,
// parsers, and placement calculations. They borrow string/vector inputs for
// each call, return owned strings/results, retain no transport state, and
// report malformed or incompatible wire data through bool/error/status while
// preserving the fixed protocol constants and message limits.

namespace HarkenSearch::ReaperBridge {

inline constexpr int kProtocolVersion = ARIS_REAPER_BRIDGE_PROTOCOL_VERSION;
inline constexpr const char* kBridgeVersion = ARIS_REAPER_BRIDGE_VERSION_STRING;
inline constexpr const char* kBridgeManifest = ARIS_REAPER_BRIDGE_MANIFEST;
inline constexpr wchar_t kDefaultPipeName[] = L"\\\\.\\pipe\\HarkenSearchReaperBridge";
inline constexpr std::size_t kMaxBatchFiles = 512;
inline constexpr std::size_t kMaxProtocolMessageBytes = 1024 * 1024;

inline std::string CurrentArchitecture() {
#if defined(_M_X64) || defined(__x86_64__)
    return "x64";
#elif defined(_M_IX86) || defined(__i386__)
    return "x86";
#elif defined(_M_ARM64) || defined(__aarch64__)
    return "arm64";
#else
    return "unknown";
#endif
}

enum class HandshakeCode {
    Ready,
    TransportUnavailable,
    VersionMismatch,
    ProtocolMismatch,
    ArchitectureMismatch,
    ManifestMismatch,
    InvalidResponse,
    ExtensionError,
};

struct HandshakeResult {
    HandshakeCode code = HandshakeCode::TransportUnavailable;
    int protocolVersion = 0;
    std::string architecture;
    std::string bridgeVersion;
    std::string manifest;
    int actionId = 0;
    std::string detail;
};

struct ItemSpan {
    double start = 0.0;
    double end = 0.0;
};

struct PlacementDecision {
    bool valid = false;
    double requestedCursor = 0.0;
    double correction = 0.0;
    double actualStart = 0.0;
    double actualEnd = 0.0;
    double nextCursor = 0.0;
    std::string error;
};

inline std::string TrimProtocolLine(std::string_view raw) {
    while (!raw.empty() && (raw.back() == '\r' || raw.back() == '\n')) {
        raw.remove_suffix(1);
    }
    return std::string(raw);
}

inline std::vector<std::string_view> SplitProtocolFields(std::string_view line) {
    std::vector<std::string_view> fields;
    std::size_t start = 0;
    for (;;) {
        const std::size_t separator = line.find('|', start);
        if (separator == std::string_view::npos) {
            fields.push_back(line.substr(start));
            break;
        }
        fields.push_back(line.substr(start, separator - start));
        start = separator + 1;
    }
    return fields;
}

inline bool ParseNonNegativeInt(std::string_view text, int& value) {
    value = 0;
    if (text.empty()) return false;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    return parsed.ec == std::errc{} &&
           parsed.ptr == text.data() + text.size() && value >= 0;
}

inline bool ParseSize(std::string_view text, std::size_t& value) {
    value = 0;
    if (text.empty()) return false;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}

inline std::string BuildHelloRequest() {
    return "HELLO|" + std::to_string(kProtocolVersion) + "|" +
           CurrentArchitecture() + "|" + kBridgeManifest + "\n";
}

inline bool ParseHelloRequest(
    std::string_view raw,
    std::string& protocolVersion,
    std::string& architecture,
    std::string& manifest)
{
    const std::string line = TrimProtocolLine(raw);
    const auto fields = SplitProtocolFields(line);
    if (fields.size() != 4 || fields[0] != "HELLO" || fields[1].empty() ||
        fields[2].empty() || fields[3].empty()) {
        return false;
    }
    protocolVersion.assign(fields[1]);
    architecture.assign(fields[2]);
    manifest.assign(fields[3]);
    return true;
}

inline std::string BuildHelloResponse(int actionId) {
    return "OK|HELLO|" + std::to_string(kProtocolVersion) + "|" +
           CurrentArchitecture() + "|" + kBridgeVersion + "|" +
           kBridgeManifest + "|" + std::to_string(actionId) + "\n";
}

inline HandshakeResult ParseHelloResponse(std::string_view raw) {
    HandshakeResult result;
    const std::string line = TrimProtocolLine(raw);
    const auto fields = SplitProtocolFields(line);
    if (fields.size() >= 2 && fields[0] == "ERR") {
        if (fields[1] == "PROTOCOL" || fields[1] == "VERSION") {
            result.code = HandshakeCode::ProtocolMismatch;
        } else if (fields[1] == "ARCH") {
            result.code = HandshakeCode::ArchitectureMismatch;
        } else if (fields[1] == "MANIFEST") {
            result.code = HandshakeCode::ManifestMismatch;
        } else {
            result.code = HandshakeCode::ExtensionError;
        }
        result.detail = line;
        return result;
    }
    if (fields.size() != 7 || fields[0] != "OK" || fields[1] != "HELLO") {
        result.code = (!fields.empty() && fields[0] == "OK")
            ? HandshakeCode::VersionMismatch
            : HandshakeCode::InvalidResponse;
        result.detail = line.empty() ? "empty handshake response" : line;
        return result;
    }
    int protocol = 0;
    int actionId = 0;
    if (!ParseNonNegativeInt(fields[2], protocol) ||
        !ParseNonNegativeInt(fields[6], actionId)) {
        result.code = HandshakeCode::InvalidResponse;
        result.detail = "invalid handshake numeric field";
        return result;
    }
    result.protocolVersion = protocol;
    result.architecture.assign(fields[3]);
    result.bridgeVersion.assign(fields[4]);
    result.manifest.assign(fields[5]);
    result.actionId = actionId;
    result.detail = line;
    if (protocol != kProtocolVersion) {
        result.code = HandshakeCode::ProtocolMismatch;
    } else if (result.bridgeVersion != kBridgeVersion) {
        result.code = HandshakeCode::VersionMismatch;
    } else if (result.architecture != CurrentArchitecture()) {
        result.code = HandshakeCode::ArchitectureMismatch;
    } else if (result.manifest != kBridgeManifest) {
        result.code = HandshakeCode::ManifestMismatch;
    } else {
        result.code = HandshakeCode::Ready;
    }
    return result;
}

inline bool BuildInsertBatchRequest(
    const std::vector<std::string>& filePaths,
    std::string& message,
    std::string& error)
{
    message.clear();
    error.clear();
    if (filePaths.empty()) {
        error = "no files selected";
        return false;
    }
    if (filePaths.size() > kMaxBatchFiles) {
        error = "too many files in one REAPER batch";
        return false;
    }
    message = "INSERT_BATCH|" + std::to_string(kProtocolVersion) + "|" +
              std::to_string(filePaths.size()) + "|";
    for (const std::string& path : filePaths) {
        if (path.empty()) {
            error = "empty file path";
            message.clear();
            return false;
        }
        if (path.find('\r') != std::string::npos || path.find('\n') != std::string::npos) {
            error = "file path contains a line break";
            message.clear();
            return false;
        }
        message += std::to_string(path.size()) + ":" + path;
        if (message.size() > kMaxProtocolMessageBytes) {
            error = "REAPER batch message is too large";
            message.clear();
            return false;
        }
    }
    message.push_back('\n');
    return true;
}

inline bool ParseInsertBatchRequest(
    std::string_view raw,
    std::vector<std::string>& filePaths,
    std::string& error)
{
    filePaths.clear();
    error.clear();
    const std::string line = TrimProtocolLine(raw);
    const std::string prefix = "INSERT_BATCH|" + std::to_string(kProtocolVersion) + "|";
    if (line.rfind(prefix, 0) != 0) {
        error = "unsupported insert protocol";
        return false;
    }
    std::size_t offset = prefix.size();
    const std::size_t countEnd = line.find('|', offset);
    std::size_t count = 0;
    if (countEnd == std::string::npos ||
        !ParseSize(std::string_view(line).substr(offset, countEnd - offset), count) ||
        count == 0 || count > kMaxBatchFiles) {
        error = "invalid batch file count";
        return false;
    }
    offset = countEnd + 1;
    filePaths.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        const std::size_t lengthEnd = line.find(':', offset);
        std::size_t length = 0;
        if (lengthEnd == std::string::npos ||
            !ParseSize(std::string_view(line).substr(offset, lengthEnd - offset), length) ||
            length == 0 || length > line.size() - (lengthEnd + 1)) {
            error = "invalid length-prefixed file path";
            filePaths.clear();
            return false;
        }
        offset = lengthEnd + 1;
        filePaths.emplace_back(line.substr(offset, length));
        offset += length;
    }
    if (offset != line.size()) {
        error = "unexpected trailing batch payload";
        filePaths.clear();
        return false;
    }
    return true;
}

inline PlacementDecision EvaluatePlacement(
    double requestedCursor,
    const std::vector<ItemSpan>& insertedSpans)
{
    PlacementDecision result;
    result.requestedCursor = requestedCursor;
    if (!std::isfinite(requestedCursor) || requestedCursor < 0.0) {
        result.error = "invalid edit cursor";
        return result;
    }
    if (insertedSpans.empty()) {
        result.error = "REAPER reported success without inserted items";
        return result;
    }
    double minimumStart = std::numeric_limits<double>::infinity();
    double maximumEnd = -std::numeric_limits<double>::infinity();
    for (const ItemSpan& span : insertedSpans) {
        if (!std::isfinite(span.start) || !std::isfinite(span.end) || span.end <= span.start) {
            result.error = "invalid inserted item span";
            return result;
        }
        minimumStart = (std::min)(minimumStart, span.start);
        maximumEnd = (std::max)(maximumEnd, span.end);
    }
    result.correction = (std::max)(0.0, requestedCursor - minimumStart);
    result.actualStart = minimumStart + result.correction;
    result.actualEnd = maximumEnd + result.correction;
    result.nextCursor = (std::max)(requestedCursor, result.actualEnd);
    result.valid = result.actualStart + 1.0e-9 >= requestedCursor &&
                   result.nextCursor + 1.0e-9 >= result.actualEnd;
    if (!result.valid) result.error = "inserted item starts before the edit cursor";
    return result;
}

inline std::string BuildInsertBatchSuccessResponse(
    int insertedFiles,
    double firstStart,
    double finalEnd)
{
    char buffer[256] = {};
    std::snprintf(buffer, sizeof(buffer), "OK|INSERT_BATCH|%d|%d|%.9f|%.9f\n",
                  kProtocolVersion, insertedFiles, firstStart, finalEnd);
    return buffer;
}

inline bool ParseInsertBatchSuccessResponse(
    std::string_view raw,
    int expectedFiles,
    std::string& error)
{
    const std::string line = TrimProtocolLine(raw);
    const auto fields = SplitProtocolFields(line);
    if (fields.size() == 6 && fields[0] == "OK" && fields[1] == "INSERT_BATCH") {
        int protocol = 0;
        int inserted = 0;
        if (ParseNonNegativeInt(fields[2], protocol) &&
            ParseNonNegativeInt(fields[3], inserted) &&
            protocol == kProtocolVersion && inserted == expectedFiles) {
            error.clear();
            return true;
        }
        error = "incompatible batch acknowledgement";
        return false;
    }
    if (line.rfind("ERR|", 0) == 0) {
        error = line.substr(4);
    } else {
        error = line.empty() ? "empty insert response" : "unexpected insert response: " + line;
    }
    return false;
}

inline bool BuildInsertRequest(
    std::string_view filePath,
    std::string& message,
    std::string& error)
{
    return BuildInsertBatchRequest({std::string(filePath)}, message, error);
}

inline bool ParseInsertRequest(
    std::string_view raw,
    std::string& filePath,
    std::string& error)
{
    std::vector<std::string> paths;
    if (!ParseInsertBatchRequest(raw, paths, error) || paths.size() != 1) return false;
    filePath = std::move(paths.front());
    return true;
}

} // namespace HarkenSearch::ReaperBridge

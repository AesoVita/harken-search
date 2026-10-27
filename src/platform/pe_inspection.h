#pragma once

#include <string>

namespace Platform::PeInspection {

// @stage7-contract
// API contract: inspection functions synchronously read the caller-owned
// UTF-8 path (and export-name/key strings) without modifying the file. Result
// strings are owned by the returned value; callers must branch on Status and
// may use systemError for diagnostics. The boundary is thread-safe at the
// API level, performs blocking IO, and reports unsupported/invalid inputs via
// status rather than exceptions.

enum class Status {
    Unsupported,
    Success,
    InvalidArgument,
    OpenFailed,
    InvalidFormat,
    ValueUnavailable,
    QueryFailed,
    EncodingFailed,
};

struct StringResult {
    Status status = Status::Unsupported;
    std::string value;
    unsigned long systemError = 0;
};

StringResult ReadArchitecture(const std::string& pathUtf8);
StringResult ReadFileVersion(const std::string& pathUtf8);
StringResult ReadVersionString(const std::string& pathUtf8,
                               const std::string& keyUtf8);

struct ExportResult {
    Status status = Status::Unsupported;
    bool present = false;
    unsigned long systemError = 0;
};

ExportResult ProbeExport(const std::string& pathUtf8,
                         const std::string& exportName);

} // namespace Platform::PeInspection

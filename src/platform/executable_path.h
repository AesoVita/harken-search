#pragma once

#include <string>

namespace Platform {

// @stage7-contract
// API contract: GetExecutablePath queries only the current process and returns
// an owned UTF-8 path. Success is complete and NUL-free; every non-success
// result has an empty path and no partial-path guarantee.

enum class ExecutablePathStatus {
    Unsupported,
    Success,
    QueryFailed,
    Truncated,
    EncodingFailed,
};

struct ExecutablePathResult {
    ExecutablePathStatus status = ExecutablePathStatus::Unsupported;
    std::string pathUtf8;
    unsigned long systemError = 0;
};

// Precondition: none; queries the current process executable.
// Postcondition: Success returns a complete UTF-8 path with no trailing NUL;
// every non-success status returns an empty path and never returns a partial.
ExecutablePathResult GetExecutablePath();

} // namespace Platform

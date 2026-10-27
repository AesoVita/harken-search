#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Platform {

// @stage7-contract
// API contract: process queries consume image names during the call and return
// owned ID/status values. Termination/wait helpers preserve explicit status and
// system-error domains; callers own any subsequent process lifecycle.

std::vector<uint32_t> FindProcessIdsByImageName(const std::string& imageName);

enum class ProcessTerminationStatus {
    Unsupported,
    Terminated,
    InvalidArgument,
    OpenFailed,
    TerminateFailed,
    WaitFailed,
};

struct ProcessTerminationResult {
    ProcessTerminationStatus status = ProcessTerminationStatus::Unsupported;
    unsigned long systemError = 0;
};

ProcessTerminationResult TerminateProcessById(
    uint32_t processId,
    uint32_t exitCode,
    uint32_t waitTimeoutMs);

} // namespace Platform

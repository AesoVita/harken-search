#pragma once

#include <cstdint>
#include <string_view>

namespace Platform {

// @stage7-contract
// API contract: launch helpers consume UTF-8 executable/argument views during
// the call and return owned status/process-id results. ProcessLaunchOptions
// controls window/handle inheritance only; callers own later process lifecycle.

enum class ProcessLaunchStatus {
    Unsupported,
    Ok,
    InvalidArgument,
    Failed,
};

struct ProcessLaunchResult {
    ProcessLaunchStatus status = ProcessLaunchStatus::Unsupported;
    uint32_t processId = 0;
    unsigned long systemError = 0;
};

struct ProcessLaunchOptions {
    bool noWindow = true;
    bool inheritHandles = false;
};

enum class ProcessRunStatus {
    Unsupported,
    Completed,
    TimedOut,
    LaunchFailed,
    WaitFailed,
};

struct ProcessRunResult {
    ProcessRunStatus status = ProcessRunStatus::Unsupported;
    int exitCode = 1;
    unsigned long systemError = 0;
};

ProcessLaunchResult LaunchProcess(
    std::wstring_view applicationName,
    std::wstring_view commandLine,
    const ProcessLaunchOptions& options);

ProcessRunResult RunProcessAndWait(
    std::wstring_view applicationName,
    std::wstring_view commandLine,
    const ProcessLaunchOptions& options,
    std::uint32_t timeoutMs,
    std::uint32_t timeoutTerminateExitCode);

} // namespace Platform

#ifdef _WIN32

#include "process_launch.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <string>
#include <vector>

#ifdef AURO_PROCESS_LAUNCH_TESTING
#include "tests/process_launch_test_seam.h"
namespace Platform::ProcessLaunchTesting {
const BackendOps* g_backendOps = nullptr;
void SetBackendOps(const BackendOps* ops) noexcept { g_backendOps = ops; }
BOOL TestCreateProcessW(
    LPCWSTR applicationName, LPWSTR commandLine,
    LPSECURITY_ATTRIBUTES processAttributes, LPSECURITY_ATTRIBUTES threadAttributes,
    BOOL inheritHandles, DWORD creationFlags, LPVOID environment,
    LPCWSTR currentDirectory, LPSTARTUPINFOW startupInfo,
    LPPROCESS_INFORMATION processInfo) {
    return g_backendOps
        ? g_backendOps->create(applicationName, commandLine, processAttributes,
              threadAttributes, inheritHandles, creationFlags, environment,
              currentDirectory, startupInfo, processInfo)
        : ::CreateProcessW(applicationName, commandLine, processAttributes,
              threadAttributes, inheritHandles, creationFlags, environment,
              currentDirectory, startupInfo, processInfo);
}
DWORD TestLastError() {
    return g_backendOps ? g_backendOps->lastError() : ::GetLastError();
}
BOOL TestClose(HANDLE handle) {
    return g_backendOps ? g_backendOps->close(handle) : ::CloseHandle(handle);
}
} // namespace Platform::ProcessLaunchTesting

#define AURO_PROCESS_LAUNCH_SEAM_ENABLE
#include "tests/process_launch_test_redirect.h"
#undef AURO_PROCESS_LAUNCH_SEAM_ENABLE
#endif

namespace Platform {

ProcessLaunchResult LaunchProcess(
    std::wstring_view applicationName,
    std::wstring_view commandLine,
    const ProcessLaunchOptions& options) {
    ProcessLaunchResult result;
    if (applicationName.empty() ||
        applicationName.find(L'\0') != std::wstring_view::npos ||
        commandLine.find(L'\0') != std::wstring_view::npos) {
        result.status = ProcessLaunchStatus::InvalidArgument;
        return result;
    }

    const std::wstring application(applicationName);
    std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
    mutableCommand.push_back(L'\0');
    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process = {};
    const BOOL ok = CreateProcessW(
        application.c_str(), mutableCommand.data(), nullptr, nullptr,
        options.inheritHandles ? TRUE : FALSE,
        options.noWindow ? CREATE_NO_WINDOW : 0,
        nullptr, nullptr, &startup, &process);
    if (!ok) {
        result.status = ProcessLaunchStatus::Failed;
        result.systemError = GetLastError();
        return result;
    }

    result.status = ProcessLaunchStatus::Ok;
    result.processId = process.dwProcessId;
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return result;
}

ProcessRunResult RunProcessAndWait(
    std::wstring_view applicationName,
    std::wstring_view commandLine,
    const ProcessLaunchOptions& options,
    std::uint32_t timeoutMs,
    std::uint32_t timeoutTerminateExitCode) {
    ProcessRunResult result;
    if (applicationName.empty() ||
        applicationName.find(L'\0') != std::wstring_view::npos ||
        commandLine.find(L'\0') != std::wstring_view::npos) {
        result.status = ProcessRunStatus::LaunchFailed;
        return result;
    }

    const std::wstring application(applicationName);
    std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
    mutableCommand.push_back(L'\0');
    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process = {};
    const BOOL ok = CreateProcessW(
        application.c_str(), mutableCommand.data(), nullptr, nullptr,
        options.inheritHandles ? TRUE : FALSE,
        options.noWindow ? CREATE_NO_WINDOW : 0,
        nullptr, nullptr, &startup, &process);
    if (!ok) {
        result.status = ProcessRunStatus::LaunchFailed;
        result.systemError = GetLastError();
        return result;
    }

    const DWORD waitResult = WaitForSingleObject(
        process.hProcess, timeoutMs == 0 ? INFINITE : timeoutMs);
    if (waitResult == WAIT_TIMEOUT) {
        if (!TerminateProcess(process.hProcess, timeoutTerminateExitCode)) {
            result.systemError = GetLastError();
        }
        WaitForSingleObject(process.hProcess, 1000);
        result.status = ProcessRunStatus::TimedOut;
    } else if (waitResult != WAIT_OBJECT_0) {
        result.status = ProcessRunStatus::WaitFailed;
        result.systemError = GetLastError();
    } else {
        result.status = ProcessRunStatus::Completed;
    }

    DWORD exitCode = 1;
    if (GetExitCodeProcess(process.hProcess, &exitCode)) {
        result.exitCode = static_cast<int>(exitCode);
    } else if (result.systemError == 0) {
        result.systemError = GetLastError();
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return result;
}

} // namespace Platform

#ifdef AURO_PROCESS_LAUNCH_TESTING
#define AURO_PROCESS_LAUNCH_SEAM_DISABLE
#include "tests/process_launch_test_redirect.h"
#undef AURO_PROCESS_LAUNCH_SEAM_DISABLE
#endif

#endif

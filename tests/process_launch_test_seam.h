#pragma once

#ifdef AURO_PROCESS_LAUNCH_TESTING

#include <windows.h>

namespace Platform::ProcessLaunchTesting {

struct BackendOps {
    BOOL (*create)(LPCWSTR applicationName, LPWSTR commandLine,
                   LPSECURITY_ATTRIBUTES processAttributes,
                   LPSECURITY_ATTRIBUTES threadAttributes, BOOL inheritHandles,
                   DWORD creationFlags, LPVOID environment, LPCWSTR currentDirectory,
                   LPSTARTUPINFOW startupInfo, LPPROCESS_INFORMATION processInfo);
    DWORD (*lastError)();
    BOOL (*close)(HANDLE handle);
};

void SetBackendOps(const BackendOps* ops) noexcept;
BOOL TestCreateProcessW(
    LPCWSTR applicationName, LPWSTR commandLine,
    LPSECURITY_ATTRIBUTES processAttributes, LPSECURITY_ATTRIBUTES threadAttributes,
    BOOL inheritHandles, DWORD creationFlags, LPVOID environment,
    LPCWSTR currentDirectory, LPSTARTUPINFOW startupInfo,
    LPPROCESS_INFORMATION processInfo);
DWORD TestLastError();
BOOL TestClose(HANDLE handle);

} // namespace Platform::ProcessLaunchTesting

#endif

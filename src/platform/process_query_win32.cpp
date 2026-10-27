#ifdef _WIN32

#include "process_query.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>

namespace Platform {
namespace {

bool EqualsIgnoreCaseWideAscii(const wchar_t* lhs, const char* rhs) {
    if (!lhs || !rhs) {
        return false;
    }
    while (*lhs && *rhs) {
        wchar_t a = *lhs++;
        char b = *rhs++;
        if (a >= L'A' && a <= L'Z') a = static_cast<wchar_t>(a - L'A' + L'a');
        if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
        if (a != static_cast<unsigned char>(b)) {
            return false;
        }
    }
    return *lhs == L'\0' && *rhs == '\0';
}

} // namespace

std::vector<uint32_t> FindProcessIdsByImageName(const std::string& imageName) {
    std::vector<uint32_t> processIds;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return processIds;
    }
    PROCESSENTRY32W entry = {};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (EqualsIgnoreCaseWideAscii(entry.szExeFile, imageName.c_str())) {
                processIds.push_back(static_cast<uint32_t>(entry.th32ProcessID));
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return processIds;
}

ProcessTerminationResult TerminateProcessById(
    uint32_t processId,
    uint32_t exitCode,
    uint32_t waitTimeoutMs) {
    ProcessTerminationResult result;
    if (processId == 0) {
        result.status = ProcessTerminationStatus::InvalidArgument;
        return result;
    }
    HANDLE process = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, processId);
    if (!process) {
        result.status = ProcessTerminationStatus::OpenFailed;
        result.systemError = GetLastError();
        return result;
    }
    if (!TerminateProcess(process, exitCode)) {
        result.status = ProcessTerminationStatus::TerminateFailed;
        result.systemError = GetLastError();
        CloseHandle(process);
        return result;
    }
    const DWORD waitResult = WaitForSingleObject(process, waitTimeoutMs);
    if (waitResult != WAIT_OBJECT_0) {
        result.status = ProcessTerminationStatus::WaitFailed;
        result.systemError = GetLastError();
        CloseHandle(process);
        return result;
    }
    result.status = ProcessTerminationStatus::Terminated;
    CloseHandle(process);
    return result;
}

} // namespace Platform

#endif

#pragma once

#include <cstdint>
#include <string>

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#endif

namespace Platform::FileTimes {

// @stage7-contract
// API contract: GetFileTimesWide synchronously borrows the path, writes both
// caller-owned Unix timestamp outputs, and closes any native handle before
// return. Outputs are reset to zero on entry; false reports empty, unsupported,
// open, or metadata-query failure without retaining resources.

inline bool GetFileTimesWide(const std::wstring& filePath,
                             int64_t& outCreationUnix,
                             int64_t& outModifiedUnix) {
    outCreationUnix = 0;
    outModifiedUnix = 0;
#ifdef _WIN32
    if (filePath.empty()) return false;

    HANDLE h = CreateFileW(filePath.c_str(), GENERIC_READ,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;

    FILETIME ftCreate{}, ftAccess{}, ftWrite{};
    const bool ok = (GetFileTime(h, &ftCreate, &ftAccess, &ftWrite) != 0);
    CloseHandle(h);
    if (!ok) return false;

    const auto ftToUnix = [](const FILETIME& ft) -> int64_t {
        ULARGE_INTEGER ui;
        ui.LowPart = ft.dwLowDateTime;
        ui.HighPart = ft.dwHighDateTime;
        const uint64_t epochDiff100ns = 116444736000000000ULL;
        if (ui.QuadPart < epochDiff100ns) return 0;
        return static_cast<int64_t>((ui.QuadPart - epochDiff100ns) / 10000000ULL);
    };

    outCreationUnix = ftToUnix(ftCreate);
    outModifiedUnix = ftToUnix(ftWrite);
    return true;
#else
    (void)filePath;
    return false;
#endif
}

} // namespace Platform::FileTimes

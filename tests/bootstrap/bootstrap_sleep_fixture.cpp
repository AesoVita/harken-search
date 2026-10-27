#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <cwchar>
#include <string>
#include <vector>

namespace {

DWORD configuredDelay() {
    DWORD delayMs = 250U;
    constexpr DWORD kConfiguredDelayCapacity = 16U;
    wchar_t configuredDelay[kConfiguredDelayCapacity]{};
    const DWORD configuredLength = GetEnvironmentVariableW(
        L"ARIS_BOOTSTRAP_SLEEP_MS", configuredDelay,
        kConfiguredDelayCapacity);
    if (configuredLength > 0U &&
        configuredLength < kConfiguredDelayCapacity) {
        wchar_t* end = nullptr;
        const unsigned long parsed = std::wcstoul(configuredDelay, &end, 10);
        if (end != configuredDelay && *end == L'\0' && parsed > 0UL &&
            parsed <= 10'000UL) {
            delayMs = static_cast<DWORD>(parsed);
        }
    }
    return delayMs;
}

}  // namespace

int wmain(const int argc, wchar_t** argv) {
    if (argc == 3 && std::wcscmp(argv[1], L"--descendant") == 0) {
        const HANDLE marker = CreateFileW(
            argv[2], GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
        if (marker == INVALID_HANDLE_VALUE) return 2;
        const DWORD processId = GetCurrentProcessId();
        DWORD written = 0U;
        const bool published = WriteFile(
            marker, &processId, sizeof(processId), &written, nullptr) != FALSE &&
            written == sizeof(processId) && FlushFileBuffers(marker) != FALSE;
        if (!published) {
            CloseHandle(marker);
            return 3;
        }
        Sleep(10'000U);
        CloseHandle(marker);
        return 0;
    }

    std::vector<wchar_t> markerPath(32768U, L'\0');
    const DWORD markerLength = GetEnvironmentVariableW(
        L"ARIS_BOOTSTRAP_DESCENDANT_MARKER", markerPath.data(),
        static_cast<DWORD>(markerPath.size()));
    if (markerLength > 0U && markerLength < markerPath.size()) {
        std::vector<wchar_t> module(32768U, L'\0');
        const DWORD moduleLength = GetModuleFileNameW(
            nullptr, module.data(), static_cast<DWORD>(module.size()));
        if (moduleLength == 0U || moduleLength >= module.size()) return 4;
        std::wstring command = L"\"" + std::wstring(module.data(), moduleLength) +
            L"\" --descendant \"" +
            std::wstring(markerPath.data(), markerLength) + L"\"";
        std::vector<wchar_t> mutableCommand(command.begin(), command.end());
        mutableCommand.push_back(L'\0');
        PROCESS_INFORMATION child{};
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        if (!CreateProcessW(
                module.data(), mutableCommand.data(), nullptr, nullptr, FALSE,
                CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT, nullptr, nullptr,
                &startup, &child)) {
            return 5;
        }
        CloseHandle(child.hThread);
        CloseHandle(child.hProcess);
    }

    Sleep(configuredDelay());
    return 0;
}

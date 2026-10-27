#include "platform/process_query.h"
#include "platform/system.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <iostream>
#include <string>

int main() {
    // GetTickCount64 on Windows has a timer tick of roughly 15.6 ms. Use a
    // sleep long enough to cross that resolution instead of asserting that a
    // 10 ms sleep must advance the millisecond counter on every scheduler run.
    constexpr uint32_t kMonotonicSleepMs = 30;
    const auto monotonicBeforeSleep = Platform::MonotonicMilliseconds();
    const auto sleepStarted = std::chrono::steady_clock::now();
    Platform::SleepMilliseconds(kMonotonicSleepMs);
    const auto sleepElapsed = std::chrono::steady_clock::now() - sleepStarted;
    const auto monotonicAfterSleep = Platform::MonotonicMilliseconds();
    if (sleepElapsed < std::chrono::milliseconds(20)) {
        std::cerr << "FAIL: system PAL sleep returned before its requested delay\n";
        return 1;
    }
    if (monotonicAfterSleep < monotonicBeforeSleep + 15) {
        std::cerr << "FAIL: system PAL monotonic clock did not advance across sleep\n";
        return 1;
    }

    char modulePath[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameA(nullptr, modulePath, MAX_PATH);
    if (length == 0 || length == MAX_PATH) {
        std::cerr << "FAIL: current process image name is unavailable\n";
        return 1;
    }
    std::string imageName(modulePath, length);
    imageName = imageName.substr(imageName.find_last_of("\\/") + 1);
    std::transform(imageName.begin(), imageName.end(), imageName.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

    const auto ids = Platform::FindProcessIdsByImageName(imageName);
    const auto current = static_cast<uint32_t>(Platform::CurrentProcessId());
    if (std::find(ids.begin(), ids.end(), current) == ids.end()) {
        std::cerr << "FAIL: case-insensitive query omitted the current process\n";
        return 1;
    }
    if (!Platform::FindProcessIdsByImageName("aris-stage6-process-that-does-not-exist.exe").empty()) {
        std::cerr << "FAIL: missing process query returned an ID\n";
        return 1;
    }
    return 0;
}

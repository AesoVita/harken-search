#ifdef _WIN32

#include "executable_path.h"
#include "text_encoding.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <string>

namespace Platform {
namespace {

constexpr std::size_t kInitialCapacity = 512;
constexpr std::size_t kMaximumCapacity = 32768;

} // namespace

ExecutablePathResult GetExecutablePath() {
    std::size_t capacity = kInitialCapacity;
    for (;;) {
        std::wstring buffer(capacity, L'\0');
        SetLastError(ERROR_SUCCESS);
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(),
                                                static_cast<DWORD>(capacity));
        if (length == 0) {
            ExecutablePathResult result;
            result.status = ExecutablePathStatus::QueryFailed;
            result.systemError = GetLastError();
            return result;
        }
        if (length < capacity) {
            buffer.resize(length);
            ExecutablePathResult result;
            result.pathUtf8 = TextEncoding::WideToUtf8Strict(buffer);
            if (result.pathUtf8.empty()) {
                result.status = ExecutablePathStatus::EncodingFailed;
                return result;
            }
            result.status = ExecutablePathStatus::Success;
            return result;
        }
        if (length >= capacity) {
            if (capacity >= kMaximumCapacity) {
                ExecutablePathResult result;
                result.status = ExecutablePathStatus::Truncated;
                result.systemError = ERROR_INSUFFICIENT_BUFFER;
                return result;
            }
            capacity = (std::min)(capacity * 2, kMaximumCapacity);
        }
    }
}

} // namespace Platform

#endif // _WIN32

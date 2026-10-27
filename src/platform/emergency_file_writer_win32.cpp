#ifdef _WIN32

#include "emergency_file_writer.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <limits>

namespace Platform::EmergencyFile {

Writer::~Writer() noexcept {
    (void)Close();
}

bool Writer::OpenOverwriteWide(const wchar_t* path) noexcept {
    (void)Close();
    if (!path) return false;
    HANDLE handle = CreateFileW(path, GENERIC_WRITE, 0, nullptr,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return false;
    native_ = reinterpret_cast<std::uintptr_t>(handle);
    return true;
}

bool Writer::WriteAll(const void* bytes, std::size_t size) noexcept {
    if (size == 0) return true;
    if (!bytes || native_ == 0) return false;
    const auto* cursor = static_cast<const unsigned char*>(bytes);
    HANDLE handle = reinterpret_cast<HANDLE>(native_);
    while (size > 0) {
        const DWORD chunk = size > std::numeric_limits<DWORD>::max()
            ? std::numeric_limits<DWORD>::max()
            : static_cast<DWORD>(size);
        DWORD written = 0;
        if (!WriteFile(handle, cursor, chunk, &written, nullptr) || written == 0) {
            return false;
        }
        cursor += written;
        size -= written;
    }
    return true;
}

bool Writer::Close() noexcept {
    if (native_ == 0) return true;
    HANDLE handle = reinterpret_cast<HANDLE>(native_);
    native_ = 0;
    return CloseHandle(handle) != FALSE;
}

} // namespace Platform::EmergencyFile

#endif

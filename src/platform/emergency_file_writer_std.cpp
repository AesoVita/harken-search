#ifndef _WIN32

#include "emergency_file_writer.h"

namespace Platform::EmergencyFile {

Writer::~Writer() noexcept {
    (void)Close();
}

bool Writer::OpenOverwriteWide(const wchar_t*) noexcept {
    return false;
}

bool Writer::WriteAll(const void*, std::size_t) noexcept {
    return false;
}

bool Writer::Close() noexcept {
    native_ = 0;
    return true;
}

} // namespace Platform::EmergencyFile

#endif

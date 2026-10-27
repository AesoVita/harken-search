#pragma once

#include <cstddef>
#include <cstdint>

namespace Platform::EmergencyFile {

// @stage7-contract
// API contract: Writer is a non-copyable, thread-confined emergency sink.
// OpenOverwriteWide takes a caller-owned NUL-terminated path and replaces the
// target; WriteAll borrows bytes only for the call; Close releases the native
// handle and must be called after a successful open. Methods return false on
// invalid state or OS failure and never throw.

class Writer {
public:
    Writer() noexcept = default;
    ~Writer() noexcept;
    Writer(const Writer&) = delete;
    Writer& operator=(const Writer&) = delete;
    Writer(Writer&&) = delete;
    Writer& operator=(Writer&&) = delete;

    bool OpenOverwriteWide(const wchar_t* path) noexcept;
    bool WriteAll(const void* bytes, std::size_t size) noexcept;
    bool Close() noexcept;

private:
    std::uintptr_t native_ = 0;
};

} // namespace Platform::EmergencyFile

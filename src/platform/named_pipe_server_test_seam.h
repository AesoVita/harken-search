#pragma once

#ifdef AURO_NAMED_PIPE_SERVER_TESTING

#include <cstdint>

namespace Platform::NamedPipe::Testing {

struct ServerBackendOps {
    std::uintptr_t (*create)(const wchar_t*, std::uint32_t, std::uint32_t,
                             std::uint32_t, unsigned long*);
    bool (*accept)(std::uintptr_t, unsigned long*);
    bool (*read)(std::uintptr_t, void*, std::uint32_t, std::uint32_t*,
                 unsigned long*);
    bool (*write)(std::uintptr_t, const void*, std::uint32_t, std::uint32_t*,
                  unsigned long*);
    bool (*flush)(std::uintptr_t, unsigned long*);
    bool (*disconnect)(std::uintptr_t, unsigned long*);
    bool (*close)(std::uintptr_t, unsigned long*);
};

void SetServerBackendOps(const ServerBackendOps* ops) noexcept;
void ResetServerBackendOps() noexcept;

} // namespace Platform::NamedPipe::Testing

#endif

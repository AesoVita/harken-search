#pragma once

#include <cstdint>

namespace Platform::NamedPipe::Testing {

struct ClientBackendOps {
    std::uintptr_t (*open)(const wchar_t* pipeName, unsigned long* error);
    bool (*wait)(const wchar_t* pipeName, std::uint32_t timeoutMs, unsigned long* error);
    void (*sleep)(std::uint32_t milliseconds);
    std::uint64_t (*clock)();
    bool (*setMode)(std::uintptr_t handle, unsigned long* error);
    bool (*write)(std::uintptr_t handle, const void* data, std::uint32_t size,
                  std::uint32_t* written, unsigned long* error);
    bool (*peek)(std::uintptr_t handle, std::uint32_t* available, unsigned long* error);
    bool (*read)(std::uintptr_t handle, void* data, std::uint32_t capacity,
                 std::uint32_t* read, unsigned long* error);
    bool (*close)(std::uintptr_t handle, unsigned long* error);
};

void SetClientBackendOps(const ClientBackendOps* ops) noexcept;
void ResetClientBackendOps() noexcept;

} // namespace Platform::NamedPipe::Testing

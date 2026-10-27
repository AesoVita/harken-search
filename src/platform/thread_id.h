#pragma once

#include <cstdint>

namespace Platform {

// @stage7-contract
// API contract: CurrentThreadId returns the calling thread's stable platform
// identifier and has no ownership or blocking side effects.

uint32_t CurrentThreadId() noexcept;

} // namespace Platform

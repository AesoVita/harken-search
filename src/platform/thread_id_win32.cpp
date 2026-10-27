#ifdef _WIN32

#include "thread_id.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace Platform {

uint32_t CurrentThreadId() noexcept {
    return static_cast<uint32_t>(GetCurrentThreadId());
}

} // namespace Platform

#endif

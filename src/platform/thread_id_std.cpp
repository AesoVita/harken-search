#ifndef _WIN32

#include "thread_id.h"

#include <functional>
#include <thread>

namespace Platform {

uint32_t CurrentThreadId() noexcept {
    return static_cast<uint32_t>(
        std::hash<std::thread::id>{}(std::this_thread::get_id()));
}

} // namespace Platform

#endif

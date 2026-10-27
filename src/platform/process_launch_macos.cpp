#ifdef __APPLE__

#include "process_launch.h"

namespace Platform {

ProcessLaunchResult LaunchProcess(
    std::wstring_view,
    std::wstring_view,
    const ProcessLaunchOptions&) {
    ProcessLaunchResult result;
    result.status = ProcessLaunchStatus::Unsupported;
    return result;
}

ProcessRunResult RunProcessAndWait(
    std::wstring_view,
    std::wstring_view,
    const ProcessLaunchOptions&,
    std::uint32_t,
    std::uint32_t) {
    ProcessRunResult result;
    result.status = ProcessRunStatus::Unsupported;
    return result;
}

} // namespace Platform

#endif

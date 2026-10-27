#include "platform/executable_path.h"

#include <utility>

namespace {
Platform::ExecutablePathResult g_result;
}

namespace CubaseExecutablePathStub {

void Reset(const std::string& testExecutablePathUtf8) {
    g_result = {};
    g_result.status = Platform::ExecutablePathStatus::Success;
    g_result.pathUtf8 = testExecutablePathUtf8;
}

void SetResult(Platform::ExecutablePathResult result) {
    g_result = std::move(result);
}

} // namespace CubaseExecutablePathStub

namespace Platform {

ExecutablePathResult GetExecutablePath() {
    return g_result;
}

} // namespace Platform

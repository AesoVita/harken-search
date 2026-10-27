#include "platform/system.h"

#include <utility>
#include <vector>

namespace {

Platform::EnvironmentStringResult g_environmentResult;
std::vector<uint32_t> g_sleepCalls;

} // namespace

namespace HostSendPlatformStub {

void Reset() {
    g_environmentResult = {};
    g_environmentResult.status = Platform::EnvironmentStringStatus::NotFound;
}

void SetEnvironmentResult(Platform::EnvironmentStringResult result) {
    g_environmentResult = std::move(result);
}

void ResetSleepCalls() {
    g_sleepCalls.clear();
}

const std::vector<uint32_t>& SleepCalls() {
    return g_sleepCalls;
}

} // namespace HostSendPlatformStub

namespace Platform {

EnvironmentStringResult ReadEnvironmentVariable(const std::string&) {
    return g_environmentResult;
}

void SleepMilliseconds(uint32_t milliseconds) {
    g_sleepCalls.push_back(milliseconds);
}

uint64_t MonotonicMilliseconds() {
    return 0;
}

} // namespace Platform

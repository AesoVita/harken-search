#include "platform/update_staging_capacity_port.h"

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

namespace Platform::UpdateStagingCapacity {
namespace {

class WindowsPort final : public aris::update::UpdateStagingCapacityPort {
public:
    aris::update::UpdateStagingCapacityResult QueryAvailableBytes() override {
        ULARGE_INTEGER freeBytes{}, totalBytes{}, totalFree{};
        if (!GetDiskFreeSpaceExW(nullptr, &freeBytes, &totalBytes, &totalFree)) {
            return {false, 0, "update staging volume unavailable"};
        }
        return {true, freeBytes.QuadPart, {}};
    }
};

}  // namespace

CreateResult CreateUpdateStagingCapacityPort() {
    auto port = std::make_unique<WindowsPort>();
    return {CreateStatus::Success, std::move(port), 0U};
}

}  // namespace Platform::UpdateStagingCapacity

#else

namespace Platform::UpdateStagingCapacity {

CreateResult CreateUpdateStagingCapacityPort() {
    return {CreateStatus::Unsupported, nullptr, 0U};
}

}  // namespace Platform::UpdateStagingCapacity

#endif

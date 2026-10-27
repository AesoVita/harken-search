#pragma once

#include "update/UpdateApplicationService.h"

#include <cstdint>
#include <memory>

namespace Platform::UpdateStagingCapacity {

enum class CreateStatus {
    Success,
    Unsupported,
};

struct CreateResult {
    CreateStatus status = CreateStatus::Unsupported;
    std::unique_ptr<aris::update::UpdateStagingCapacityPort> port;
    std::uint32_t systemError = 0;

    bool ok() const { return status == CreateStatus::Success && port != nullptr; }
};

CreateResult CreateUpdateStagingCapacityPort();

}  // namespace Platform::UpdateStagingCapacity

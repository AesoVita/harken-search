#pragma once

#include "update/UpdateHttpTransport.h"

#include <memory>

namespace Platform::UpdateHttp {

enum class CreateStatus {
    Success,
    Unsupported,
    SessionOpenFailed,
};

struct CreateResult {
    CreateStatus status = CreateStatus::Unsupported;
    std::uint32_t systemError = 0;
    std::unique_ptr<aris::update::HttpTransportPort> port;
    bool ok() const { return status == CreateStatus::Success && port != nullptr; }
};

// Production factory owns WinINet session policy and accepts no filesystem root.
CreateResult CreateUpdateHttpTransportPort();

}  // namespace Platform::UpdateHttp

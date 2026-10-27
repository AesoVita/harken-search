#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace Platform::UpdateBootstrapHealth {

enum class Status {
    Accepted,
    Unsupported,
    InvalidChannel,
    Timeout,
    IoFailure,
};

struct Result {
    Status status = Status::Unsupported;
    std::string detail;

    bool ok() const { return status == Status::Accepted; }
};

class Port {
public:
    virtual ~Port() = default;
    virtual Result CaptureChallenge() = 0;
    virtual Result AcknowledgeReady() = 0;
};

struct CreateResult {
    Status status = Status::Unsupported;
    std::unique_ptr<Port> port;
    std::uint32_t systemError = 0;

    bool ok() const { return status != Status::Unsupported && port != nullptr; }
};

CreateResult CreateUpdateBootstrapHealthPort();

}  // namespace Platform::UpdateBootstrapHealth

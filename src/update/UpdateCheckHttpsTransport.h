#pragma once

#include "update/UpdateCheckTransport.h"

#include <memory>
#include <string>
#include <vector>

namespace aris::update {

// Hard adapter ceiling: manifest/signature bodies are small control-plane
// payloads; caller caps above 16 MiB (including UINT64_MAX) fail before read
// buffer allocation.
inline constexpr std::size_t kUpdateCheckHttpsMaximumBodyBytes =
    16U * 1024U * 1024U;
inline constexpr std::uint64_t kUpdateCheckHttpsReadTimeoutMs = 1'000U;

// @stage7-contract
// API contract: the factory exposes only the dedicated owned-byte check port;
// native HTTPS handles and response buffers remain private to the adapter.

// Pure policy shared by the concrete HTTPS adapter and its boundary tests.
bool IsApprovedUpdateCheckUrl(const std::string& url, bool initialRequest);
bool IsCompanyCloudStableManifestUrl(const std::string& url);
bool IsCompanyCloudBootstrapPackageUrl(const std::string& url);
UpdateCheckFetchError ClassifyUpdateCheckHttpStatus(
    std::uint32_t statusCode);

enum class UpdateCheckReadStatus {
    Chunk,
    End,
    Network,
    Aborted,
};

struct UpdateCheckReadResult {
    UpdateCheckReadStatus status = UpdateCheckReadStatus::Network;
    std::vector<std::uint8_t> bytes;
};

class UpdateCheckHttpsReadPort {
public:
    virtual ~UpdateCheckHttpsReadPort() = default;
    virtual UpdateCheckReadResult Read(std::size_t maxBytes,
                                       std::uint64_t timeoutMs) = 0;
    virtual void Abort() noexcept = 0;
};

// Validates the hard cap before allocating a body buffer and registers the
// caller-owned abort barrier for every blocking read.
UpdateCheckFetchResult ReadUpdateCheckHttpsBody(
    UpdateCheckHttpsReadPort& port,
    const UpdateCheckCancellation& cancellation,
    std::size_t requestedBodyBytes);

enum class UpdateCheckHttpsCreateStatus {
    Success,
    Unsupported,
    SessionOpenFailed,
};

struct UpdateCheckHttpsCreateResult {
    UpdateCheckHttpsCreateStatus status =
        UpdateCheckHttpsCreateStatus::Unsupported;
    std::uint32_t systemError = 0;
    std::unique_ptr<UpdateCheckTransportPort> port;

    bool ok() const {
        return status == UpdateCheckHttpsCreateStatus::Success &&
               port != nullptr;
    }
};

// Production boundary. It owns native HTTPS requests and returns only bounded,
// owned bytes through the dedicated check transport port.
UpdateCheckHttpsCreateResult CreateUpdateCheckHttpsTransport();

}  // namespace aris::update

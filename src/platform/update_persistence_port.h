#pragma once

#include "update/UpdatePersistencePort.h"

#include <cstdint>
#include <memory>
#include <string>

namespace Platform::UpdatePersistence {

// @stage7-contract
// API contract: production creation discovers the trusted per-user update
// root from the operating system. No caller-provided root or native handle is
// accepted or exposed.

enum class CreateStatus {
    Success,
    Unsupported,
    RootUnavailable,
    EncodingFailed,
    DirectoryCreateFailed,
};

struct CreateResult {
    CreateStatus status = CreateStatus::Unsupported;
    std::unique_ptr<aris::update::UpdatePersistencePort> port;
    std::uint32_t systemError = 0;

    bool ok() const { return status == CreateStatus::Success && port != nullptr; }
};

CreateResult CreateUpdatePersistencePort();

// Converts startup creation failures into a stable diagnostic consumed by the
// update centre.  Kept beside the platform status so no caller has to guess
// what a numeric Win32 failure means.
std::string DescribeCreateFailure(CreateStatus status, std::uint32_t systemError);

// Record one bounded Bootstrap failure. The adapter owns module/OS path
// discovery, byte formatting and the trusted-root atomic write; callers cannot
// inject a root or arbitrary payload.
void RecordBootstrapFailureDiagnostic(const char* stageName, int exitCode,
                                      const char* extra);

}  // namespace Platform::UpdatePersistence

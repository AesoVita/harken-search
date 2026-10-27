#pragma once

#include "update/UpdateApplicationService.h"

#include <cstdint>
#include <memory>
#include <string>

namespace Platform::UpdateBootstrapAuthority {

enum class CreateStatus {
    Success,
    Unsupported,
    RootUnavailable,
    PolicyUnavailable,
};

struct CreateResult {
    CreateStatus status = CreateStatus::Unsupported;
    std::unique_ptr<aris::update::UpdateBootstrapAuthorityPort> port;
    std::uint32_t systemError = 0;

    bool ok() const {
        return status == CreateStatus::Success && port != nullptr;
    }
};

CreateResult CreateUpdateBootstrapAuthorityPort();

// Where the updater owns its download, staging and transaction directories.
//
// This must land on the same volume as the install: the filesystem port
// requires one shared volume serial across install/staging/transaction so the
// final swap can be an atomic rename.  Pinning the base to %LOCALAPPDATA%
// meant any install off the system drive failed at filesystem-begin with exit
// code 30, every time, and reinstalling to the same drive could not fix it.
//
// installDirectory is the resolved install root; localAppData is the
// %LOCALAPPDATA% path used both for system-drive installs and as the fallback
// when the install path has no usable drive letter (UNC, empty).  Pure and
// separately testable: no filesystem access, no known-folder lookups.
std::string ResolveUpdaterOwnedBaseForInstall(
    const std::string& installDirectory, const std::string& localAppData);

}  // namespace Platform::UpdateBootstrapAuthority

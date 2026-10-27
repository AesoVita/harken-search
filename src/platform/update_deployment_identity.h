#pragma once

#include "bootstrap/HarkenSearchBootstrapPlan.h"

#include <optional>
#include <string>
#include <vector>

namespace aris::platform {

struct InstalledRegistration {
    std::string appId;
    std::string root;
    std::uint32_t registryView = 0;
    std::uint32_t hive = 0;
};

struct DeploymentIdentitySnapshot {
    DeploymentMode mode = DeploymentMode::Portable;
    std::string openedRoot;
    std::string executablePath;
    DeploymentFileIdentity openedRootIdentity;
    DeploymentFileIdentity oldExecutable;
    bool rootWritable = false;
    bool rootIsSystemDirectory = false;
    bool rootIsReparsePoint = false;
    std::vector<InstalledRegistration> registry;
};

struct DeploymentIdentity {
    DeploymentMode mode = DeploymentMode::Portable;
    std::string installationId;
    std::string targetRoot;
    DeploymentFileIdentity openedRootIdentity;
    DeploymentFileIdentity oldExecutable;
    std::string appId;
};

enum class DeploymentIdentityError {
    None,
    InvalidMode,
    RegistryMissing,
    RegistryConflict,
    RegistryRootMismatch,
    PortableAppIdPresent,
    RootUnavailable,
    RootNotWritable,
    RootSystemDirectory,
    RootReparsePoint,
    ExecutableMismatch,
    IdentityUnstable,
};

struct DeploymentIdentityResult {
    DeploymentIdentityError error = DeploymentIdentityError::None;
    std::optional<DeploymentIdentity> identity;

    bool ok() const noexcept {
        return error == DeploymentIdentityError::None && identity.has_value();
    }
};

DeploymentIdentityResult ResolveDeploymentIdentity(
    const DeploymentIdentitySnapshot& snapshot);

DeploymentIdentitySnapshot ObserveDeploymentIdentity();

}  // namespace aris::platform

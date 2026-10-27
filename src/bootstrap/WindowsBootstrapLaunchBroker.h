#pragma once

#include "bootstrap/HarkenSearchBootstrapRuntime.h"

#include <cstdint>
#include <memory>

namespace aris::bootstrap {

// Opaque, one-shot Windows capabilities established by the authenticated
// updater before Bootstrap enters the filesystem transaction. Handle values
// are inherited/duplicated through an allowlist and never accepted from a
// command line or loggable payload.
struct WindowsBootstrapLaunchBrokerCapability {
    std::uintptr_t channelHandle = 0;
    std::uintptr_t brokerProcessHandle = 0;
    std::uintptr_t containmentJobHandle = 0;
    BootstrapProcessIdentity brokerIdentity;
    std::uint32_t launchTimeoutMs = 0;

    bool valid() const noexcept {
        return channelHandle != 0U && brokerProcessHandle != 0U &&
               containmentJobHandle != 0U && brokerIdentity.valid &&
               launchTimeoutMs != 0U;
    }
};

// Consumes every supplied handle on both success and failure. The returned
// port may issue at most two ordered brokered launches (updated image, then a
// rollback/recovery relaunch); a copied/replayed capability is rejected by the
// protocol or by its terminated exact process lifetime.
std::unique_ptr<BootstrapProcessPort>
CreateBrokeredNativeBootstrapProcessPort(
    WindowsBootstrapLaunchBrokerCapability& capability);

// Runs the non-interactive broker side of the inherited private channel. The
// mode selector is non-secret; all launch authority remains in the inherited
// handle, exact broker lifetime, request nonce, and controller-owned jobs.
int RunWindowsBootstrapLaunchBroker(std::uintptr_t inheritedChannelHandle);

}  // namespace aris::bootstrap

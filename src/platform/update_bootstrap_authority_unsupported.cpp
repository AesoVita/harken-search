#include "platform/update_bootstrap_authority_port.h"

#ifndef _WIN32

namespace Platform::UpdateBootstrapAuthority {

CreateResult CreateUpdateBootstrapAuthorityPort() {
    return {CreateStatus::Unsupported, nullptr, 0U};
}

}  // namespace Platform::UpdateBootstrapAuthority

#endif

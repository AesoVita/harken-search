#include "update/UpdateCheckHttpsTransport.h"

#ifndef _WIN32

namespace aris::update {

UpdateCheckHttpsCreateResult CreateUpdateCheckHttpsTransport() {
    UpdateCheckHttpsCreateResult result;
    result.status = UpdateCheckHttpsCreateStatus::Unsupported;
    return result;
}

}  // namespace aris::update

#endif

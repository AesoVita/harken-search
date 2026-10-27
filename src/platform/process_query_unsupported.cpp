#ifndef _WIN32

#include "process_query.h"

namespace Platform {

std::vector<uint32_t> FindProcessIdsByImageName(const std::string&) {
    return {};
}

ProcessTerminationResult TerminateProcessById(uint32_t, uint32_t, uint32_t) {
    ProcessTerminationResult result;
    result.status = ProcessTerminationStatus::Unsupported;
    return result;
}

} // namespace Platform

#endif

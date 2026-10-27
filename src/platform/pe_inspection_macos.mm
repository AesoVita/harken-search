#ifdef __APPLE__

#include "pe_inspection.h"

namespace Platform::PeInspection {

StringResult ReadArchitecture(const std::string& /*pathUtf8*/) {
    StringResult result;
    result.status = Status::Unsupported;
    return result;
}

StringResult ReadFileVersion(const std::string& /*pathUtf8*/) {
    StringResult result;
    result.status = Status::Unsupported;
    return result;
}

StringResult ReadVersionString(const std::string& /*pathUtf8*/,
                               const std::string& /*keyUtf8*/) {
    StringResult result;
    result.status = Status::Unsupported;
    return result;
}

ExportResult ProbeExport(const std::string& /*pathUtf8*/,
                         const std::string& /*exportName*/) {
    ExportResult result;
    result.status = Status::Unsupported;
    return result;
}

} // namespace Platform::PeInspection

#endif // __APPLE__

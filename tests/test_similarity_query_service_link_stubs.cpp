#include "../src/platform/file_prefix_probe.h"

#ifdef _WIN32

namespace Platform::FileSystem::Internal {

FilePrefixProbeResult ProbeFilePrefix(const std::string&) {
    return {};
}

} // namespace Platform::FileSystem::Internal

#endif

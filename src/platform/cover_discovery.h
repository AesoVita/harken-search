#pragma once

#include <string>

namespace Platform::CoverDiscovery {

// @stage7-contract
// API contract: Find synchronously inspects caller-owned UTF-8 folders and
// returns an owned selected path/status. It retains no paths, may perform
// bounded filesystem traversal according to useDeepSearch/maxDepth, and
// reports absence/probe failure through Status rather than exceptions.

enum class Status { Found, NotFound, ProbeError };

struct Result {
    Status status = Status::NotFound;
    std::string selectedPathUtf8;
};

Result Find(const std::string& folderPathUtf8,
            const std::string& libraryRootUtf8,
            bool useDeepSearch,
            int maxDepth);

#ifdef ARIS_COVER_DISCOVERY_TESTING
namespace Testing { void FailAfterEntries(int count); }
#endif

} // namespace Platform::CoverDiscovery

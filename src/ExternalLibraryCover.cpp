#include "ExternalLibraryCover.h"
#include <string>

namespace ExternalLibraryCover {

static std::string s_dbRootPathUtf8;

void setDbPath(const std::string& dbRootPathUtf8) {
    s_dbRootPathUtf8 = dbRootPathUtf8;
}

bool tryGetCoverForFolder(const std::string& folderPathUtf8, std::string& outCoverPathUtf8) {
    (void)folderPathUtf8;
    outCoverPathUtf8.clear();
    if (s_dbRootPathUtf8.empty()) return false;
    // No external-library adapter is configured; callers fall back to local discovery.
    return false;
}
}

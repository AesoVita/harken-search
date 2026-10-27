#include "DatabasePathNormalizationPolicy.h"

#include "Utils.h"

namespace aura::database {

std::string NormalizeFolderPathForDb(const std::string& pathUtf8) {
    if (pathUtf8.empty()) return pathUtf8;
    try {
        auto path = Utf8ToPath(pathUtf8);
        return PathToUtf8(path);
    } catch (...) {
        return pathUtf8;
    }
}

}  // namespace aura::database

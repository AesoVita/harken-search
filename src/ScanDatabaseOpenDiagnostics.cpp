#include "ScanDatabaseOpenDiagnostics.h"
#include <algorithm>
namespace aura {
namespace {
std::string TrimForDiagnostic(const std::string& value, size_t limit) {
    if (value.size() <= limit) return value;
    return value.substr(0, limit);
}
}
std::string FormatScanDatabaseOpenFailure(const std::string& databasePath, const std::string& error, const std::vector<std::string>& folders) {
    const std::string shownError = TrimForDiagnostic(error.empty() ? "open scan database failed" : error, 80);
    const std::string shownFolder = TrimForDiagnostic(folders.empty() ? "empty" : folders.front(), 60);
    const std::string prefix = "ScanDatabaseOpenFailed: error=" + shownError + " folder=" + shownFolder + " path=";
    const size_t pathLimit = prefix.size() >= 191 ? 0 : 191 - prefix.size();
    return prefix + TrimForDiagnostic(databasePath.empty() ? "empty" : databasePath, pathLimit);
}
}

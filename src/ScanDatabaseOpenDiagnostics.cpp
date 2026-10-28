#include "ScanDatabaseOpenDiagnostics.h"
namespace aura {
std::string FormatScanDatabaseOpenFailure(const std::string& databasePath, const std::string& error, const std::vector<std::string>& folders) {
    std::string detail = "ScanDatabaseOpenFailed: path=";
    detail += databasePath.empty() ? "empty" : databasePath;
    detail += " error=";
    detail += error.empty() ? "open scan database failed" : error;
    detail += " folder=";
    detail += folders.empty() ? "empty" : folders.front();
    return detail;
}
}

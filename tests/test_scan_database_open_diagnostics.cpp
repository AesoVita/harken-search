#include "ScanDatabaseOpenDiagnostics.h"
#include <iostream>
#include <string>
#include <vector>
static int g_failed = 0;
#define ASSERT(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << (msg) << "\n"; ++g_failed; } else { std::cout << "PASS: " << (msg) << "\n"; } } while (0)
int main() {
    const std::string detail = aura::FormatScanDatabaseOpenFailure("", "open scan database failed", {"H:/Khron Studio"});
    ASSERT(detail.find("path=empty") != std::string::npos, "empty database path is visible");
    ASSERT(detail.find("error=open scan database failed") != std::string::npos, "failure text is visible");
    ASSERT(detail.find("folder=H:/Khron Studio") != std::string::npos, "scan folder is visible");
    const std::string longPath(180, 'D');
    const std::string longDetail = aura::FormatScanDatabaseOpenFailure(longPath, "open scan database failed", {"H:/Khron Studio"});
    ASSERT(longDetail.size() <= 191, "diagnostic detail stays within logger capacity");
    ASSERT(longDetail.find("error=open scan database failed") != std::string::npos, "long path keeps failure text");
    ASSERT(longDetail.find("folder=H:/Khron Studio") != std::string::npos, "long path keeps scan folder");
    return g_failed == 0 ? 0 : 1;
}

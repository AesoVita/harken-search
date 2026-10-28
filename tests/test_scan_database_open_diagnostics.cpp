#include "ScanDatabaseOpenDiagnostics.h"
#include <iostream>
#include <string>
#include <vector>
static int g_failed = 0;
#define ASSERT(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << (msg) << "\n"; ++g_failed; } else { std::cout << "PASS: " << (msg) << "\n"; } } while (0)
int main() {
    const std::string detail = aura::FormatScanDatabaseOpenFailure("", "open scan database failed", {"H:\\Khron Studio"});
    ASSERT(detail.find("path=empty") != std::string::npos, "empty database path is visible");
    ASSERT(detail.find("error=open scan database failed") != std::string::npos, "failure text is visible");
    ASSERT(detail.find("folder=H:\\Khron Studio") != std::string::npos, "scan folder is visible");
    return g_failed == 0 ? 0 : 1;
}

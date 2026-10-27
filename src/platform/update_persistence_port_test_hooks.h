#pragma once

#include "platform/update_persistence_port.h"

#ifdef ARIS_UPDATE_PERSISTENCE_TESTING

#include <string>

namespace Platform::UpdatePersistence::Testing {

// Test-only root injection. This declaration and its implementation are absent
// from production builds.
CreateResult CreateForTrustedRoot(const std::string& rootPathUtf8);

// Test-only resolver seam. Production callers cannot choose either root or
// diagnostic bytes; this hook exercises C/D/E and missing-root behavior without
// writing to the developer machine's updater directory.
aris::update::UpdatePersistenceResult WriteBootstrapFailureDiagnosticForInstall(
    const std::string& installDirectory, const std::string& localAppData,
    const char* stageName, int exitCode, const char* extra);

}  // namespace Platform::UpdatePersistence::Testing

#endif

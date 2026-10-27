#pragma once

#include <string>

#include "bootstrap/HarkenSearchBootstrapMain.h"

namespace aris::bootstrap {

enum class BootstrapExitOutcome {
    Succeeded,
    // Nothing was replaced; running the update again is the right next step.
    Retryable,
    // The previous version was restored and is usable as-is.
    RolledBack,
    // The release itself was refused; a local reinstall changes nothing.
    ReleaseRejected,
    // The install is no longer in a known-good shape.
    Unrecoverable,
};

struct BootstrapExitExplanation {
    BootstrapExitOutcome outcome = BootstrapExitOutcome::Succeeded;
    // Only true when reinstalling is genuinely the fix.  Every nonzero exit
    // used to claim this, which is why "I reinstalled and it still fails" was
    // such a common report: reinstalling does not fix a rolled-back update, a
    // stray process holding the install, or a rejected release.
    bool requiresReinstall = false;
    // False for success and for developer/unsupported builds.
    bool showDialog = false;
    // User-facing text, always carrying the raw exit code so support can act.
    std::string message;
};

// Pure mapping from a Bootstrap exit code to what the user should be told.
BootstrapExitExplanation ExplainBootstrapExit(int exitCode);

// Record which stage refused, so a failed update is diagnosable from the
// user's machine instead of only from a demo build.  Writes
// beside patch-info.json in the updater-owned base for the installation
// volume.  Only a nonzero failure replaces the record; successful and
// cleanup-pending outcomes preserve the last real failure.  Deliberately
// records only the stage name, exit code and a short extra token -- no paths,
// no user data.  Never throws; a failure to record must not change the update
// outcome.
void RecordBootstrapStageFailure(const char* stageName, int exitCode,
                                 const char* extra);

// Shared path and write boundaries used by the runtime entry above.  Keeping
// these explicit lets the resolver be checked without changing the process or
// trusting a path supplied by an update plan.
std::string ResolveBootstrapStageFailureRecordPath(
    const std::string& installDirectory, const std::string& localAppData);

// Resolves the record's absolute path, or an empty string when the location
// cannot be determined.  Exposed for tests.
std::string BootstrapStageFailureRecordPath();

}  // namespace aris::bootstrap

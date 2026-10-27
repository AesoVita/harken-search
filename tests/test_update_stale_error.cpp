// A persisted failure used to outlive the problem it described.
//
// UpdatePhase::Error is written to patch-info.json and restored verbatim on
// the next launch, with nothing deciding whether it still applies.  Observed
// on 2026-08-11: an install that had failed in the morning with
// "update check redirect is not approved" kept showing that red line hours
// later, after the very bug had been fixed and after a *successful* upgrade to
// 1.0.24 -- because the stale record was still on disk.
//
// Two independent reasons a restored Error can be obsolete:
//   1. The recorded failure belongs to an older product version, so whatever
//      refused was in code the user is no longer running.
//   2. A ready patch for a newer version is present, i.e. the pipeline got
//      further than the recorded failure did.
//
// A stale record must not be presented as the current state.  It is still
// worth keeping as history, so this only governs what the UI shows.

#include "update/HarkenSearchUpdateComposition.h"

#include <cstdio>
#include <string>

namespace {

int g_failures = 0;

void check(bool condition, const std::string& what) {
    if (!condition) {
        ++g_failures;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

}  // namespace

int main() {
    using namespace aris::update;

    // A failure recorded by the version currently running is current: the code
    // that refused is the code the user has, so keep showing it.
    check(!IsStaleUpdateFailure("1.0.24", "1.0.24", ""),
          "a failure recorded by the running version is shown");

    // Pure helper contract: a failure recorded by an older build is stale.
    // The integrated composition cannot supply recordedVersion yet.
    check(IsStaleUpdateFailure("1.0.23", "1.0.24", ""),
          "a failure recorded by an older version is stale");
    check(IsStaleUpdateFailure("1.0.9", "1.0.24", ""),
          "version comparison is numeric, not lexicographic");

    // The exact case seen on 2026-08-11: failed at 1.0.23, upgraded to 1.0.24,
    // and the morning's redirect error was still on screen.
    check(IsStaleUpdateFailure("1.0.23", "1.0.24", ""),
          "the observed post-upgrade stale error is suppressed");

    // Integrated-path contract: with no persisted recording version, a newer
    // ready patch still proves the pipeline got past the failure.
    check(IsStaleUpdateFailure("", "1.0.24", "1.0.25"),
          "a failure is stale once a newer patch is ready");
    check(!IsStaleUpdateFailure("", "1.0.24", "1.0.24"),
          "a ready patch for the same version does not invalidate the failure");

    // Missing or malformed data must not silently hide a real failure: when
    // staleness cannot be established, keep showing it.
    check(!IsStaleUpdateFailure("", "1.0.24", ""),
          "an unknown recording version is not assumed stale");
    check(!IsStaleUpdateFailure("1.0.23", "", ""),
          "an unknown running version is not assumed stale");
    check(!IsStaleUpdateFailure("not-a-version", "1.0.24", ""),
          "an unparseable recording version is not assumed stale");
    check(!IsStaleUpdateFailure("1.0.24", "not-a-version", ""),
          "an unparseable running version is not assumed stale");

    // A newer recording version than the running build is odd (downgrade or
    // tampering).  Do not treat it as stale -- hiding failures on unexpected
    // input is the wrong direction to fail.
    check(!IsStaleUpdateFailure("1.0.25", "1.0.24", ""),
          "a newer recording version does not suppress the failure");

    if (g_failures == 0) std::printf("all stale update failure checks pass\n");
    return g_failures == 0 ? 0 : 1;
}

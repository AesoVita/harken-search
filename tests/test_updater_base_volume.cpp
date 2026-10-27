// Aris could never self-update when installed off the system drive.
//
// The updater-owned base was hardcoded to %LOCALAPPDATA%\Aris\updates, which
// always lives on the system volume.  WindowsBootstrapFilesystemPort::Begin
// then requires the install root, staging root and transaction root to share
// one volume serial, so an install on D:\ or E:\ failed with
// RootVolumeMismatch -> exit code 30 (filesystem-begin-failed) on every single
// attempt.  The installer shows the directory page and does not pin a drive,
// so this hit anyone who picked a non-system drive -- and reinstalling to the
// same drive could never fix it.
//
// The same-volume requirement itself is worth keeping: it is what allows the
// atomic rename-based replacement.  The fix is to place the staging area on
// the install volume rather than to relax the check.

#include "platform/update_bootstrap_authority_port.h"

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

std::string VolumePrefix(const std::string& path) {
    if (path.size() >= 2 && path[1] == ':') return path.substr(0, 2);
    return {};
}

}  // namespace

int main() {
    using namespace Platform::UpdateBootstrapAuthority;

    const std::string localAppData = "C:\\Users\\Someone\\AppData\\Local";

    // Installed on the system drive: unchanged behaviour, stays under
    // %LOCALAPPDATA% so existing installs keep their update directory.
    const std::string onC = ResolveUpdaterOwnedBaseForInstall(
        "C:\\Program Files\\Aris", localAppData);
    check(!onC.empty(), "a system-drive install resolves a base");
    check(VolumePrefix(onC) == "C:",
          "a system-drive install keeps its base on the system volume");
    check(onC.find("AppData") != std::string::npos,
          "a system-drive install still uses the LocalAppData location");

    // Installed off the system drive: the base must follow the install volume,
    // otherwise Begin() rejects the update before touching anything.
    const std::string onE =
        ResolveUpdaterOwnedBaseForInstall("E:\\Tools\\Aris", localAppData);
    check(!onE.empty(), "a non-system-drive install resolves a base");
    check(VolumePrefix(onE) == "E:",
          "a non-system-drive install puts its base on the install volume");

    const std::string onD =
        ResolveUpdaterOwnedBaseForInstall("D:\\Apps\\Aris", localAppData);
    check(VolumePrefix(onD) == "D:",
          "the base follows whichever volume the install uses");
    check(onD != onE, "different install volumes get different bases");

    // The base must never sit inside the install directory: Begin() also
    // rejects overlapping roots.
    check(onE.rfind("E:\\Tools\\Aris", 0) != 0,
          "the base never nests inside the install directory itself");

    // Lower-case drive letters are the same volume, not a different one.
    const std::string lower =
        ResolveUpdaterOwnedBaseForInstall("e:\\Tools\\Aris", localAppData);
    check(!lower.empty() && (VolumePrefix(lower) == "E:" || VolumePrefix(lower) == "e:"),
          "drive letter case does not change the resolved volume");

    // Degenerate inputs must not produce a path on the wrong volume; falling
    // back to LocalAppData is correct only when the install path is unusable.
    const std::string unknown = ResolveUpdaterOwnedBaseForInstall("", localAppData);
    check(VolumePrefix(unknown) == "C:",
          "an unresolvable install path falls back to the LocalAppData base");

    // UNC installs have no drive letter; falling back is honest here because a
    // network install cannot satisfy the same-volume rule anyway.
    const std::string unc = ResolveUpdaterOwnedBaseForInstall(
        "\\\\server\\share\\Aris", localAppData);
    check(!unc.empty(), "a UNC install still resolves some base");

#if defined(ARIS_PREVIEW_ISOLATION) && ARIS_PREVIEW_ISOLATION && defined(ARIS_PREVIEW_ISOLATION_ROOT_UTF8)
    const std::string preview = ARIS_PREVIEW_ISOLATION_ROOT_UTF8;
    check(ResolveUpdaterOwnedBaseForInstall(preview + "/qa", localAppData) == preview + "/updates",
          "preview downloads stay in the compile-time preview root");
    check(ResolveUpdaterOwnedBaseForInstall(preview + "-other/qa", localAppData) != preview + "/updates",
          "a sibling path sharing the prefix does not inherit preview state");
    check(ResolveUpdaterOwnedBaseForInstall(preview, localAppData) != preview + "/updates",
          "updater root never nests inside an install at the preview root itself");
#endif

    if (g_failures == 0) std::printf("all updater base volume checks pass\n");
    return g_failures == 0 ? 0 : 1;
}

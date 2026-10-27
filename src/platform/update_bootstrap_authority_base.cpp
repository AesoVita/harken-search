#include "platform/update_bootstrap_authority_port.h"

#include <cctype>
#include <algorithm>
#include <string>

namespace Platform::UpdateBootstrapAuthority {
namespace {

// "E:\Tools\Aris" -> "E:", "" when there is no drive letter (UNC, relative).
std::string DriveOf(const std::string& path) {
    if (path.size() < 2 || path[1] != ':') return {};
    const unsigned char letter = static_cast<unsigned char>(path[0]);
    if (std::isalpha(letter) == 0) return {};
    std::string drive;
    drive += static_cast<char>(std::toupper(letter));
    drive += ':';
    return drive;
}

bool SameDrive(const std::string& left, const std::string& right) {
    const std::string a = DriveOf(left);
    const std::string b = DriveOf(right);
    return !a.empty() && a == b;
}

}  // namespace

std::string ResolveUpdaterOwnedBaseForInstall(
    const std::string& installDirectory, const std::string& localAppData) {
#if defined(ARIS_PREVIEW_ISOLATION) && ARIS_PREVIEW_ISOLATION && defined(ARIS_PREVIEW_ISOLATION_ROOT_UTF8)
    // A branch QA package already has a compile-time isolated data root. Keep
    // its downloaded notices/archive there too, rather than touching the
    // installed product's shared per-drive update state during UI acceptance.
    const auto normalize = [](std::string path) {
        std::replace(path.begin(), path.end(), '\\', '/');
        std::transform(path.begin(), path.end(), path.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        while (!path.empty() && path.back() == '/') path.pop_back();
        return path;
    };
    const std::string preview = normalize(ARIS_PREVIEW_ISOLATION_ROOT_UTF8);
    const std::string installed = normalize(installDirectory);
    if (!preview.empty() && installed.compare(0, preview.size() + 1, preview + "/") == 0)
        return std::string(ARIS_PREVIEW_ISOLATION_ROOT_UTF8) + "/updates";
#endif
    const std::string localAppDataBase =
        localAppData.empty() ? std::string()
                             : localAppData + "\\HarkenSearch\\updates";

    // System-drive installs keep the historical location so existing installs
    // keep their staged downloads and journals.
    if (installDirectory.empty() || SameDrive(installDirectory, localAppData)) {
        return localAppDataBase;
    }

    const std::string drive = DriveOf(installDirectory);
    if (drive.empty()) {
        // No drive letter (UNC or relative).  Such an install cannot satisfy
        // the same-volume rule under any base, so fall back rather than invent
        // a path; the failure is then reported honestly instead of silently
        // writing somewhere unexpected.
        return localAppDataBase;
    }

    // Off-system-drive install: put the updater's directories on the install
    // volume so install/staging/transaction share one volume serial.  A
    // top-level per-drive directory is used rather than a path inside the
    // install directory, because the filesystem port also rejects roots that
    // overlap the install root.
    return drive + "\\ProgramData\\HarkenSearch\\updates";
}

}  // namespace Platform::UpdateBootstrapAuthority

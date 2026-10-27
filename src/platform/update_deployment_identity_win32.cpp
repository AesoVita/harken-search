#include "platform/update_deployment_identity.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <sstream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <fileapi.h>
#include <winreg.h>
#endif

namespace aris::platform {
namespace {

std::string lower(std::string value) {
    for (char& c : value) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return value;
}

bool samePath(const std::string& left, const std::string& right) {
    const auto normalize = [](const std::string& value) {
        auto path = std::filesystem::path(value).lexically_normal();
        while (path.has_parent_path() && path.filename().empty()) {
            const auto parent = path.parent_path();
            if (parent == path) break;
            path = parent;
        }
        return lower(path.u8string());
    };
    return normalize(left) == normalize(right);
}

std::string identityToken(const DeploymentFileIdentity& identity) {
    static constexpr char digits[] = "0123456789abcdef";
    std::ostringstream out;
    out << std::hex << identity.volumeSerial << '-';
    for (const auto byte : identity.fileId) {
        out << digits[byte >> 4U] << digits[byte & 0x0fU];
    }
    return out.str();
}

}  // namespace

DeploymentIdentityResult ResolveDeploymentIdentity(
    const DeploymentIdentitySnapshot& snapshot) {
    DeploymentIdentityResult result;
    if (snapshot.openedRoot.empty() || snapshot.executablePath.empty() ||
        !snapshot.openedRootIdentity.valid || !snapshot.oldExecutable.valid) {
        result.error = DeploymentIdentityError::RootUnavailable;
        return result;
    }
    const auto parent = std::filesystem::path(snapshot.executablePath).parent_path().u8string();
    if (!samePath(parent, snapshot.openedRoot) ||
        !samePath(snapshot.oldExecutable.canonicalPath, snapshot.executablePath)) {
        result.error = DeploymentIdentityError::ExecutableMismatch;
        return result;
    }
    if (snapshot.oldExecutable.canonicalPath.empty() ||
        snapshot.openedRootIdentity.canonicalPath.empty()) {
        result.error = DeploymentIdentityError::IdentityUnstable;
        return result;
    }

    if (snapshot.mode == DeploymentMode::Installed) {
        std::vector<const InstalledRegistration*> matches;
        for (const auto& entry : snapshot.registry) {
            if (entry.appId == kArisInstalledAppId) matches.push_back(&entry);
        }
        if (matches.empty()) {
            result.error = DeploymentIdentityError::RegistryMissing;
            return result;
        }
        if (matches.size() != 1U) {
            result.error = DeploymentIdentityError::RegistryConflict;
            return result;
        }
        if (!samePath(matches.front()->root, snapshot.openedRoot)) {
            result.error = DeploymentIdentityError::RegistryRootMismatch;
            return result;
        }
        DeploymentIdentity identity;
        identity.mode = DeploymentMode::Installed;
        identity.installationId = "installed-a8b3c9d1-e4f5-4a6b-8c9d-0e1f2a3b4c5d";
        identity.targetRoot = snapshot.openedRoot;
        identity.openedRootIdentity = snapshot.openedRootIdentity;
        identity.oldExecutable = snapshot.oldExecutable;
        identity.appId = kArisInstalledAppId;
        result.identity = std::move(identity);
        return result;
    }

    if (snapshot.mode != DeploymentMode::Portable) {
        result.error = DeploymentIdentityError::InvalidMode;
        return result;
    }
    for (const auto& entry : snapshot.registry) {
        if (entry.appId == kArisInstalledAppId) {
            result.error = DeploymentIdentityError::PortableAppIdPresent;
            return result;
        }
    }
    if (!snapshot.rootWritable) {
        result.error = DeploymentIdentityError::RootNotWritable;
        return result;
    }
    if (snapshot.rootIsSystemDirectory) {
        result.error = DeploymentIdentityError::RootSystemDirectory;
        return result;
    }
    if (snapshot.rootIsReparsePoint) {
        result.error = DeploymentIdentityError::RootReparsePoint;
        return result;
    }
    DeploymentIdentity identity;
    identity.mode = DeploymentMode::Portable;
    identity.installationId = std::string("portable-") + identityToken(snapshot.openedRootIdentity);
    identity.targetRoot = snapshot.openedRoot;
    identity.openedRootIdentity = snapshot.openedRootIdentity;
    identity.oldExecutable = snapshot.oldExecutable;
    result.identity = std::move(identity);
    return result;
}

DeploymentIdentitySnapshot ObserveDeploymentIdentity() {
    DeploymentIdentitySnapshot snapshot;
#ifdef _WIN32
    std::wstring buffer(32768U, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0U || length >= buffer.size()) return snapshot;
    buffer.resize(length);
    const std::filesystem::path executable(buffer);
    snapshot.executablePath = executable.u8string();
    snapshot.openedRoot = executable.parent_path().u8string();
    snapshot.mode = DeploymentMode::Portable;
    HANDLE file = CreateFileW(executable.wstring().c_str(), FILE_READ_ATTRIBUTES,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (file == INVALID_HANDLE_VALUE) return snapshot;
    FILE_ID_INFO fileId{};
    FILE_ATTRIBUTE_TAG_INFO tags{};
    std::vector<wchar_t> executablePath(512U, L'\0');
    DWORD executableLength = GetFinalPathNameByHandleW(
        file, executablePath.data(), static_cast<DWORD>(executablePath.size()),
        FILE_NAME_NORMALIZED);
    if (executableLength >= executablePath.size()) {
        executablePath.resize(executableLength + 1U);
        executableLength = GetFinalPathNameByHandleW(
            file, executablePath.data(),
            static_cast<DWORD>(executablePath.size()), FILE_NAME_NORMALIZED);
    }
    const bool metadata = GetFileInformationByHandleEx(file, FileIdInfo, &fileId, sizeof(fileId)) != FALSE &&
                          GetFileInformationByHandleEx(file, FileAttributeTagInfo, &tags, sizeof(tags)) != FALSE &&
                          executableLength != 0U;
    CloseHandle(file);
    if (!metadata || (tags.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U) return snapshot;
    std::wstring canonicalExecutable(executablePath.data(), executableLength);
    if (canonicalExecutable.rfind(L"\\\\?\\", 0U) == 0U) {
        canonicalExecutable.erase(0U, 4U);
    }
    snapshot.executablePath =
        std::filesystem::path(canonicalExecutable).u8string();
    snapshot.oldExecutable.canonicalPath = snapshot.executablePath;
    snapshot.oldExecutable.volumeSerial = fileId.VolumeSerialNumber;
    std::copy(std::begin(fileId.FileId.Identifier), std::end(fileId.FileId.Identifier), snapshot.oldExecutable.fileId.begin());
    snapshot.oldExecutable.valid = true;
    HANDLE root = CreateFileW(
        executable.parent_path().wstring().c_str(),
        FILE_READ_ATTRIBUTES | FILE_ADD_FILE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    snapshot.rootWritable = root != INVALID_HANDLE_VALUE;
    if (root == INVALID_HANDLE_VALUE) {
        root = CreateFileW(
            executable.parent_path().wstring().c_str(), FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    }
    if (root == INVALID_HANDLE_VALUE) return snapshot;
    FILE_ID_INFO rootId{};
    FILE_ATTRIBUTE_TAG_INFO rootTags{};
    std::vector<wchar_t> rootPath(512U, L'\0');
    DWORD rootLength = GetFinalPathNameByHandleW(
        root, rootPath.data(), static_cast<DWORD>(rootPath.size()),
        FILE_NAME_NORMALIZED);
    if (rootLength >= rootPath.size()) {
        rootPath.resize(rootLength + 1U);
        rootLength = GetFinalPathNameByHandleW(
            root, rootPath.data(), static_cast<DWORD>(rootPath.size()),
            FILE_NAME_NORMALIZED);
    }
    const bool rootMetadata = GetFileInformationByHandleEx(
                                  root, FileIdInfo, &rootId, sizeof(rootId)) != FALSE &&
                              GetFileInformationByHandleEx(
                                  root, FileAttributeTagInfo, &rootTags,
                                  sizeof(rootTags)) != FALSE &&
                              rootLength != 0U;
    CloseHandle(root);
    if (!rootMetadata) return snapshot;
    snapshot.rootIsReparsePoint =
        (rootTags.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U;
    std::wstring canonicalRoot(rootPath.data(), rootLength);
    if (canonicalRoot.rfind(L"\\\\?\\", 0U) == 0U) canonicalRoot.erase(0U, 4U);
    snapshot.openedRoot = std::filesystem::path(canonicalRoot).u8string();
    snapshot.openedRootIdentity.canonicalPath = snapshot.openedRoot;
    snapshot.openedRootIdentity.volumeSerial = rootId.VolumeSerialNumber;
    std::copy(std::begin(rootId.FileId.Identifier),
              std::end(rootId.FileId.Identifier),
              snapshot.openedRootIdentity.fileId.begin());
    snapshot.openedRootIdentity.valid = !snapshot.openedRoot.empty() &&
        snapshot.openedRootIdentity.volumeSerial != 0U &&
        !snapshot.rootIsReparsePoint;
    wchar_t windowsPath[MAX_PATH]{};
    const UINT windowsLength = GetWindowsDirectoryW(windowsPath, MAX_PATH);
    if (windowsLength != 0U && windowsLength < MAX_PATH) {
        const std::string systemRoot = lower(
            std::filesystem::path(windowsPath).lexically_normal().u8string());
        const std::string opened = lower(
            std::filesystem::path(snapshot.openedRoot).lexically_normal().u8string());
        snapshot.rootIsSystemDirectory = opened == systemRoot ||
            (opened.size() > systemRoot.size() &&
             opened.rfind(systemRoot + "\\", 0U) == 0U);
    }
    // Inno Setup unescapes only the leading `{{` in AppId but not the trailing
    // `}}`, and always appends `_is1` to the registry subkey. The current
    // compiled AppId is `{{31B6...}}`, so the actual key becomes `{31B6...}}_is1`.
    // See `installer/Aris_Setup.iss` `MyAppId`.
    constexpr const wchar_t* kUninstall =
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{31B6E215-3D2B-43EA-88C8-72D73ABC9A0C}}_is1";
    for (HKEY hive : {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER}) {
        for (const REGSAM view : {KEY_WOW64_32KEY, KEY_WOW64_64KEY}) {
            HKEY key = nullptr;
            if (RegOpenKeyExW(hive, kUninstall, 0U, KEY_READ | view, &key) != ERROR_SUCCESS) {
                continue;
            }
            wchar_t rootBuffer[32768]{};
            DWORD bytes = sizeof(rootBuffer);
            DWORD type = 0U;
            const bool read = RegQueryValueExW(key, L"InstallLocation", nullptr, &type,
                                                reinterpret_cast<LPBYTE>(rootBuffer), &bytes) == ERROR_SUCCESS &&
                              (type == REG_SZ || type == REG_EXPAND_SZ) && bytes >= sizeof(wchar_t);
            if (read) {
                std::wstring registryRoot(
                    rootBuffer, (bytes / sizeof(wchar_t)) - 1U);
                snapshot.registry.push_back({kArisInstalledAppId,
                                             std::filesystem::path(registryRoot).u8string(),
                                             static_cast<std::uint32_t>(view == KEY_WOW64_32KEY ? 32U : 64U),
                                             static_cast<std::uint32_t>(hive == HKEY_LOCAL_MACHINE ? 1U : 2U)});
            }
            RegCloseKey(key);
        }
    }
    if (!snapshot.registry.empty()) snapshot.mode = DeploymentMode::Installed;
#endif
    return snapshot;
}

}  // namespace aris::platform

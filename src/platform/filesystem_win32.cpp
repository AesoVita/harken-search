#ifdef _WIN32

#include "filesystem.h"
#include "file_prefix_probe.h"
#include "text_encoding.inl"

#include <algorithm>
#include <string_view>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#ifdef AURO_FILESYSTEM_ATOMIC_REPLACE_TESTING
#include "filesystem_atomic_replace_test_hooks.h"
#endif
#ifdef AURO_FILESYSTEM_FILE_PREFIX_PROBE_TESTING
#include "file_prefix_probe_test_hooks.h"
#endif
#ifdef AURO_FILESYSTEM_PROBE_PATH_TESTING
#include "filesystem_probe_path_test_hooks.h"
#endif

#ifdef AURO_FILESYSTEM_ATOMIC_REPLACE_TESTING
namespace {
Platform::FileSystem::Testing::AtomicReplaceScript* g_atomicReplaceScript = nullptr;
}
#endif
#ifdef AURO_FILESYSTEM_FILE_PREFIX_PROBE_TESTING
namespace {
Platform::FileSystem::Testing::FilePrefixProbeScript* g_filePrefixProbeScript = nullptr;
}
#endif
#ifdef AURO_FILESYSTEM_PROBE_PATH_TESTING
namespace {
Platform::FileSystem::Testing::ProbePathScript* g_probePathScript = nullptr;
Platform::FileSystem::Testing::DriveTypeScript* g_driveTypeScript = nullptr;
Platform::FileSystem::Testing::NetworkPathForceScript* g_networkForceScript = nullptr;
}
#endif

namespace Platform::FileSystem {

#ifdef AURO_FILESYSTEM_FILE_PREFIX_PROBE_TESTING
namespace Testing {
void SetFilePrefixProbeScript(FilePrefixProbeScript* script) {
    g_filePrefixProbeScript = script;
}
} // namespace Testing
#endif

#ifdef AURO_FILESYSTEM_ATOMIC_REPLACE_TESTING
namespace Testing {
void SetAtomicReplaceScript(AtomicReplaceScript* script) {
    g_atomicReplaceScript = script;
}
} // namespace Testing
#endif

#ifdef AURO_FILESYSTEM_PROBE_PATH_TESTING
namespace Testing {
void SetProbePathScript(ProbePathScript* script) {
    g_probePathScript = script;
}
void SetDriveTypeScript(DriveTypeScript* script) {
    g_driveTypeScript = script;
}
void SetNetworkPathForceScript(NetworkPathForceScript* script) {
    g_networkForceScript = script;
}
} // namespace Testing
#endif

namespace {

bool LooksLikeNetworkPathUtf8(const std::string& pathUtf8) {
    std::string_view path = pathUtf8;
    if (path.size() >= 4 && path[0] == '\\' && path[1] == '\\' && path[2] == '?' && path[3] == '\\') {
        path.remove_prefix(4);
        if (path.size() >= 4 &&
            (path[0] == 'U' || path[0] == 'u') &&
            (path[1] == 'N' || path[1] == 'n') &&
            (path[2] == 'C' || path[2] == 'c') &&
            path[3] == '\\') {
            return true;
        }
    } else if (path.size() >= 2 && path[0] == '\\' && path[1] == '\\') {
        return true;
    } else if (path.size() >= 2 && path[0] == '/' && path[1] == '/') {
        return true;
    }
    if (path.size() >= 2 &&
        ((path[0] >= 'A' && path[0] <= 'Z') ||
         (path[0] >= 'a' && path[0] <= 'z')) &&
        path[1] == ':') {
        const char root[] = {path[0], ':', '\\', '\0'};
#ifdef AURO_FILESYSTEM_PROBE_PATH_TESTING
        if (g_driveTypeScript && g_driveTypeScript->overrideDriveType) {
            ++g_driveTypeScript->calls;
            return g_driveTypeScript->driveType == DRIVE_REMOTE ||
                   g_driveTypeScript->driveType == DRIVE_NO_ROOT_DIR;
        }
#endif
        const UINT driveType = GetDriveTypeA(root);
        return driveType == DRIVE_REMOTE || driveType == DRIVE_NO_ROOT_DIR;
    }
    return false;
}

bool IsNetworkUnreachableWin32Error(DWORD error, const std::string& pathUtf8) {
    switch (error) {
    case ERROR_BAD_NETPATH:
    case ERROR_BAD_NET_NAME:
    case ERROR_NETWORK_UNREACHABLE:
    case ERROR_NO_NET_OR_BAD_PATH:
    case ERROR_LOGON_FAILURE:
    case ERROR_UNEXP_NET_ERR:
    case ERROR_NOT_READY:
    case ERROR_NETWORK_ACCESS_DENIED:
    case ERROR_NO_NETWORK:
    case ERROR_BAD_DEVICE:
    case ERROR_DEV_NOT_EXIST:
        return true;
    case ERROR_ACCESS_DENIED:
    case ERROR_PATH_NOT_FOUND:
        return LooksLikeNetworkPathUtf8(pathUtf8);
    default:
        return false;
    }
}

} // namespace

bool IsNetworkPath(const std::string& pathUtf8) {
#ifdef AURO_FILESYSTEM_PROBE_PATH_TESTING
    if (g_networkForceScript) {
        ++g_networkForceScript->calls;
        if (g_networkForceScript->forceAll) return true;
        if (!g_networkForceScript->prefix.empty() &&
            pathUtf8.size() >= g_networkForceScript->prefix.size() &&
            pathUtf8.compare(0, g_networkForceScript->prefix.size(), g_networkForceScript->prefix) == 0) {
            return true;
        }
    }
#endif
    return LooksLikeNetworkPathUtf8(pathUtf8);
}

PathStatus ProbePath(const std::string& pathUtf8, std::string* errorMessage) {
    if (errorMessage) errorMessage->clear();
    if (pathUtf8.empty()) return PathStatus::Missing;

    const std::wstring widePath = TextEncoding::Utf8ToWide(pathUtf8);
    if (widePath.empty()) {
        if (errorMessage) *errorMessage = "Invalid UTF-8 path";
        return PathStatus::Error;
    }

    DWORD attributes = INVALID_FILE_ATTRIBUTES;
    DWORD lastError = 0;
#ifdef AURO_FILESYSTEM_PROBE_PATH_TESTING
    if (g_probePathScript) {
        ++g_probePathScript->calls;
        if (g_probePathScript->delayMs > 0) {
            Sleep(static_cast<DWORD>(g_probePathScript->delayMs));
        }
        attributes = g_probePathScript->attributes;
        lastError = g_probePathScript->lastError;
    } else
#endif
    {
        attributes = GetFileAttributesW(widePath.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES) lastError = GetLastError();
    }

    if (attributes != INVALID_FILE_ATTRIBUTES) return PathStatus::Present;
    if (lastError == ERROR_FILE_NOT_FOUND) return PathStatus::Missing;
    if (IsNetworkUnreachableWin32Error(lastError, pathUtf8)) {
        if (errorMessage) *errorMessage = "network path unreachable";
        return PathStatus::Unreachable;
    }
    if (lastError == ERROR_PATH_NOT_FOUND) return PathStatus::Missing;
    if (errorMessage) *errorMessage = "GetFileAttributesW failed";
    return PathStatus::Error;
}

PathAttributes QueryPathAttributes(const std::string& pathUtf8) {    if (pathUtf8.empty()) return {};
    const std::wstring path = TextEncoding::Utf8ToWide(pathUtf8);
    if (path.empty()) return {};
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) return {};
    return {
        (attributes & FILE_ATTRIBUTE_HIDDEN) != 0,
        (attributes & FILE_ATTRIBUTE_SYSTEM) != 0,
        (attributes & FILE_ATTRIBUTE_TEMPORARY) != 0,
    };
}

namespace Internal {

FilePrefixProbeResult ProbeFilePrefix(const std::string& pathUtf8) {
    FilePrefixProbeResult result;

#ifdef AURO_FILESYSTEM_FILE_PREFIX_PROBE_TESTING
    if (g_filePrefixProbeScript) {
        auto& script = *g_filePrefixProbeScript;
        ++script.openCalls;
        if (!script.openResult) {
            result.nativeOpenError = script.nativeOpenError;
            return result;
        }

        result.opened = true;
        ++script.sizeCalls;
        result.sizeAvailable = script.sizeResult;
        if (script.sizeResult) result.sizeBytes = script.sizeBytes;

        ++script.readCalls;
        result.readSucceeded = script.readResult;
        result.bytesRead = (std::min)(script.bytesRead, result.prefix.size());
        std::copy_n(script.prefix.begin(), result.bytesRead, result.prefix.begin());
        ++script.closeCalls;
        return result;
    }
#endif

    const std::wstring widePath = TextEncoding::Utf8ToWide(pathUtf8);
    HANDLE file = CreateFileW(widePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        result.nativeOpenError = GetLastError();
        return result;
    }

    result.opened = true;
    LARGE_INTEGER size{};
    result.sizeAvailable = GetFileSizeEx(file, &size) != FALSE;
    if (result.sizeAvailable) result.sizeBytes = static_cast<uint64_t>(size.QuadPart);

    DWORD bytesRead = 0;
    result.readSucceeded = ReadFile(file, result.prefix.data(),
                                    static_cast<DWORD>(result.prefix.size()),
                                    &bytesRead, nullptr) != FALSE;
    result.bytesRead = (std::min)(static_cast<size_t>(bytesRead), result.prefix.size());
    CloseHandle(file);
    return result;
}

} // namespace Internal

bool AtomicFileReplace(const std::string& targetPathUtf8, const std::string& sourcePathUtf8) {
    std::wstring wTarget = TextEncoding::Utf8ToWide(targetPathUtf8);
    std::wstring wSource = TextEncoding::Utf8ToWide(sourcePathUtf8);
#ifdef AURO_FILESYSTEM_ATOMIC_REPLACE_TESTING
    if (g_atomicReplaceScript) {
        auto& script = *g_atomicReplaceScript;
        ++script.moveFileExCalls;
        if (script.moveFileExResult) {
            script.targetPresent = true;
            script.sourcePresent = false;
            return true;
        }
    } else
#endif
    if (MoveFileExW(wSource.c_str(), wTarget.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        return true;
    }
    return false;
}

} // namespace Platform::FileSystem

#endif // _WIN32

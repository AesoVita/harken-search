#include "platform/harken_user_data_layout.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>
#include <cstdlib>

namespace Platform::HarkenUserData {
namespace {

#if defined(ARIS_PREVIEW_ISOLATION) && ARIS_PREVIEW_ISOLATION
#if defined(ARIS_PREVIEW_ISOLATION_ROOT_UTF8)
const char kCompileTimePreviewRootUtf8[] = ARIS_PREVIEW_ISOLATION_ROOT_UTF8;
#else
#error "Preview isolation requires an explicit ARIS_PREVIEW_ISOLATION_ROOT_UTF8"
#endif
#endif

std::wstring ReadEnvWide(const wchar_t* name) {
    wchar_t* value = nullptr;
    size_t length = 0;
    if (_wdupenv_s(&value, &length, name) != 0 || value == nullptr || value[0] == L'\0') {
        if (value) std::free(value);
        return {};
    }
    std::wstring out(value);
    std::free(value);
    while (!out.empty() && (out.back() == L'\\' || out.back() == L'/')) out.pop_back();
    return out;
}

std::wstring KnownFolderWide(const KNOWNFOLDERID& folder) {
    PWSTR path = nullptr;
    if (SHGetKnownFolderPath(folder, 0, nullptr, &path) != S_OK || path == nullptr) {
        if (path) CoTaskMemFree(path);
        return {};
    }
    std::wstring out(path);
    CoTaskMemFree(path);
    while (!out.empty() && (out.back() == L'\\' || out.back() == L'/')) out.pop_back();
    return out;
}

bool EnvFlagEnabled(const wchar_t* name) {
    const std::wstring value = ReadEnvWide(name);
    if (value.empty()) return false;
    std::wstring lower = value;
    for (wchar_t& ch : lower) {
        if (ch >= L'A' && ch <= L'Z') ch = static_cast<wchar_t>(ch - L'A' + L'a');
    }
    return lower == L"1" || lower == L"true" || lower == L"yes" || lower == L"on";
}

std::wstring ToLowerWide(std::wstring value) {
    for (wchar_t& ch : value) {
        if (ch >= L'A' && ch <= L'Z') ch = static_cast<wchar_t>(ch - L'A' + L'a');
    }
    return value;
}

std::wstring StripExtendedPrefix(std::wstring value) {
    if (value.size() >= 4 && ToLowerWide(value.substr(0, 4)) == L"\\\\?\\") {
        value.erase(0, 4);
    }
    return value;
}

bool CanonicalizeWide(const std::wstring& input, std::wstring& output) {
    if (input.empty()) return false;
    wchar_t buf[32768] = {};
    const DWORD n = GetFullPathNameW(input.c_str(), 32768, buf, nullptr);
    if (n == 0 || n >= 32768) return false;
    output = StripExtendedPrefix(buf);
    while (!output.empty() && (output.back() == L'\\' || output.back() == L'/')) output.pop_back();
    return !output.empty();
}

bool HasReparseInAncestry(const std::wstring& canonical) {
    if (canonical.empty()) return true;
    std::wstring cur = canonical;
    for (;;) {
        const DWORD attr = GetFileAttributesW(cur.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_REPARSE_POINT)) {
            return true;
        }
        const size_t slash = cur.find_last_of(L"\\/");
        if (slash == std::wstring::npos || slash < 2) break;
        if (slash == 2 && cur.size() >= 2 && cur[1] == L':') break;
        cur.resize(slash);
        if (cur.size() == 2 && cur[1] == L':') cur += L'\\';
    }
    return false;
}

bool EqualCanonicalWide(const std::wstring& left, const std::wstring& right) {
    return ToLowerWide(left) == ToLowerWide(right);
}

bool CanonicalIsUnderRoot(const std::wstring& child, const std::wstring& root) {
    const std::wstring c = ToLowerWide(child);
    const std::wstring r = ToLowerWide(root);
    if (c.empty() || r.empty()) return false;
    if (c == r) return true;
    return c.size() > r.size() && c.compare(0, r.size(), r) == 0 && c[r.size()] == L'\\';
}

bool TryCanonicalIsolationRoot(std::wstring& rootOut) {
    if (!CanonicalizeWide(Utf8ToWidePath(PreviewIsolationRootUtf8()), rootOut)) return false;
    if (HasReparseInAncestry(rootOut)) return false;
    return true;
}

bool TryExactControlledDir(const std::wstring& root, const wchar_t* relative, std::wstring& out) {
    if (!CanonicalizeWide(root + relative, out)) return false;
    if (HasReparseInAncestry(out)) return false;
    if (!CanonicalIsUnderRoot(out, root)) return false;
    return true;
}

void EnsureDirectoryDeepWide(const std::wstring& path) {
    if (path.empty()) return;
    SHCreateDirectoryExW(nullptr, path.c_str(), nullptr);
}

}  // namespace

bool PreviewIsolationActive() {
#if defined(ARIS_PREVIEW_ISOLATION) && ARIS_PREVIEW_ISOLATION
    return true;
#else
    return EnvFlagEnabled(L"ARIS_PREVIEW_ISOLATION");
#endif
}

std::string PreviewIsolationRootUtf8() {
#if defined(ARIS_PREVIEW_ISOLATION) && ARIS_PREVIEW_ISOLATION
    return kCompileTimePreviewRootUtf8;
#else
    return {};
#endif
}

bool PreviewIsolationAllowsDataPathUtf8(const std::string& pathUtf8) {
    if (!PreviewIsolationActive()) return true;
    if (pathUtf8.empty()) return false;
    std::wstring root;
    if (!TryCanonicalIsolationRoot(root)) return false;
    std::wstring child;
    if (!CanonicalizeWide(Utf8ToWidePath(pathUtf8), child)) return false;
    if (HasReparseInAncestry(child)) return false;
    return CanonicalIsUnderRoot(child, root);
}

void ApplyPreviewIsolationProcessEnvironment() {
    if (!PreviewIsolationActive()) return;
    std::wstring root;
    if (!TryCanonicalIsolationRoot(root)) return;
    std::wstring local;
    std::wstring roaming;
    std::wstring temp;
    std::wstring runtime;
    if (!TryExactControlledDir(root, L"\\profile\\Local", local)) return;
    if (!TryExactControlledDir(root, L"\\profile\\Roaming", roaming)) return;
    if (!CanonicalizeWide(root + L"\\temp", temp) || HasReparseInAncestry(temp) ||
        !CanonicalIsUnderRoot(temp, root)) {
        return;
    }
    if (!CanonicalizeWide(root + L"\\profile\\runtime", runtime) || HasReparseInAncestry(runtime) ||
        !CanonicalIsUnderRoot(runtime, root)) {
        return;
    }
    EnsureDirectoryDeepWide(local);
    EnsureDirectoryDeepWide(roaming);
    EnsureDirectoryDeepWide(temp);
    EnsureDirectoryDeepWide(runtime);
    if (HasReparseInAncestry(local) || HasReparseInAncestry(roaming) ||
        HasReparseInAncestry(temp) || HasReparseInAncestry(runtime)) {
        return;
    }
    _wputenv_s(L"LOCALAPPDATA", local.c_str());
    _wputenv_s(L"APPDATA", roaming.c_str());
    _wputenv_s(L"TEMP", temp.c_str());
    _wputenv_s(L"TMP", temp.c_str());
    _wputenv_s(L"AURA_RUNTIME_ROOT", runtime.c_str());
    _wputenv_s(L"ARIS_PREVIEW_ISOLATION", L"1");
}

std::string WideToUtf8Path(const std::wstring& value) {
    if (value.empty()) return {};
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, value.c_str(),
                                          static_cast<int>(value.size()),
                                          nullptr, 0, nullptr, nullptr);
    if (bytes <= 0) return {};
    std::string out(static_cast<size_t>(bytes), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()),
                        out.data(), bytes, nullptr, nullptr);
    return out;
}

Roots ResolveRoots() {
    Roots roots;
    if (PreviewIsolationActive()) {
        std::wstring root;
        std::wstring pinnedLocal;
        std::wstring pinnedRoaming;
        if (!TryCanonicalIsolationRoot(root) ||
            !TryExactControlledDir(root, L"\\profile\\Local", pinnedLocal) ||
            !TryExactControlledDir(root, L"\\profile\\Roaming", pinnedRoaming)) {
            return roots;
        }
        std::wstring envLocalCanon;
        const std::wstring envLocal = ReadEnvWide(L"LOCALAPPDATA");
        if (!envLocal.empty() && CanonicalizeWide(envLocal, envLocalCanon) &&
            !HasReparseInAncestry(envLocalCanon) &&
            EqualCanonicalWide(envLocalCanon, pinnedLocal)) {
            roots.localAppData = pinnedLocal;
        } else {
            roots.localAppData = pinnedLocal;
        }
        std::wstring envRoamingCanon;
        const std::wstring envRoaming = ReadEnvWide(L"APPDATA");
        if (!envRoaming.empty() && CanonicalizeWide(envRoaming, envRoamingCanon) &&
            !HasReparseInAncestry(envRoamingCanon) &&
            EqualCanonicalWide(envRoamingCanon, pinnedRoaming)) {
            roots.roamingAppData = pinnedRoaming;
        } else {
            roots.roamingAppData = pinnedRoaming;
        }
        return roots;
    }
    roots.localAppData = ReadEnvWide(L"LOCALAPPDATA");
    if (roots.localAppData.empty()) {
        roots.localAppData = KnownFolderWide(FOLDERID_LocalAppData);
    }
    roots.roamingAppData = ReadEnvWide(L"APPDATA");
    if (roots.roamingAppData.empty()) {
        roots.roamingAppData = KnownFolderWide(FOLDERID_RoamingAppData);
    }
    return roots;
}

std::wstring CanonicalRoot(const Roots& roots) {
    if (roots.localAppData.empty()) return {};
    return roots.localAppData + L"\\HarkenSearch";
}

std::wstring CanonicalDatabaseDir(const Roots& roots) {
    const std::wstring root = CanonicalRoot(roots);
    if (root.empty()) return {};
    return root + L"\\Database";
}

std::wstring CanonicalSettingsDir(const Roots& roots) {
    return CanonicalRoot(roots);
}

std::wstring CanonicalUpdatesDir(const Roots& roots) {
    const std::wstring root = CanonicalRoot(roots);
    if (root.empty()) return {};
    return root + L"\\updates";
}


std::wstring Utf8ToWidePath(const std::string& value) {
    if (value.empty()) return {};
    const int chars = MultiByteToWideChar(CP_UTF8, 0, value.c_str(),
                                          static_cast<int>(value.size()),
                                          nullptr, 0);
    if (chars <= 0) return {};
    std::wstring out(static_cast<size_t>(chars), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()),
                        out.data(), chars);
    return out;
}

std::string JoinExeFileUtf8(const std::string& exeDir, const char* fileName) {
    if (!fileName) return {};
    std::string dir = exeDir;
    while (!dir.empty() && (dir.back() == '\\' || dir.back() == '/')) dir.pop_back();
    if (dir.empty()) return fileName;
    return dir + "\\" + fileName;
}
}  // namespace Platform::HarkenUserData

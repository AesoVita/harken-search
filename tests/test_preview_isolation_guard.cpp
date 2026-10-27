#include "platform/harken_user_data_layout.h"

#if !defined(ARIS_PREVIEW_ISOLATION) || !ARIS_PREVIEW_ISOLATION || !defined(ARIS_PREVIEW_ISOLATION_ROOT_UTF8)
#error "This isolation test must never run without compile-time isolation and a synthetic root"
#endif

#include <cstdio>
#include <filesystem>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shlobj.h>

namespace fs = std::filesystem;
using Platform::HarkenUserData::PreviewIsolationActive;
using Platform::HarkenUserData::PreviewIsolationAllowsDataPathUtf8;
using Platform::HarkenUserData::PreviewIsolationRootUtf8;
using Platform::HarkenUserData::ResolveRoots;
using Platform::HarkenUserData::ResetLayoutCacheForTests;
using Platform::HarkenUserData::EnsureLayout;
using Platform::HarkenUserData::LayoutKind;
using Platform::HarkenUserData::WideToUtf8Path;
using Platform::HarkenUserData::Utf8ToWidePath;

int g_failures = 0;

void check(bool cond, const char* what) {
    if (!cond) {
        ++g_failures;
        std::printf("FAIL: %s\n", what);
    } else {
        std::printf("PASS: %s\n", what);
    }
}

std::string PreviewRootUtf8() {
#if defined(ARIS_PREVIEW_ISOLATION_ROOT_UTF8)
    return ARIS_PREVIEW_ISOLATION_ROOT_UTF8;
#else
    return PreviewIsolationRootUtf8();
#endif
}

std::wstring KnownFolderLocalAppData() {
    PWSTR path = nullptr;
    if (SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path) != S_OK || path == nullptr) {
        if (path) CoTaskMemFree(path);
        return {};
    }
    std::wstring out(path);
    CoTaskMemFree(path);
    return out;
}

bool PathIsUnderUtf8(const std::string& child, const std::string& root) {
    auto norm = [](std::string value) {
        for (char& ch : value) {
            if (ch == '/') ch = '\\';
            if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
        }
        while (!value.empty() && value.back() == '\\') value.pop_back();
        return value;
    };
    const std::string c = norm(child);
    const std::string r = norm(root);
    if (c == r) return true;
    return c.size() > r.size() && c.compare(0, r.size(), r) == 0 && c[r.size()] == '\\';
}

void ClearAppDataEnv() {
    _wputenv_s(L"LOCALAPPDATA", L"");
    _wputenv_s(L"APPDATA", L"");
    ResetLayoutCacheForTests();
}

void TestCompileIsolationStaysOnWithoutEnv() {
    _wputenv_s(L"ARIS_PREVIEW_ISOLATION", L"");
    check(PreviewIsolationActive(), "compile isolation stays on when env missing");
    _wputenv_s(L"ARIS_PREVIEW_ISOLATION", L"0");
    check(PreviewIsolationActive(), "compile isolation stays on when env=0");
}

void TestMissingEnvDoesNotUseKnownFolder() {
    ClearAppDataEnv();
    const auto roots = ResolveRoots();
    const std::string local = WideToUtf8Path(roots.localAppData);
    const std::string roaming = WideToUtf8Path(roots.roamingAppData);
    const std::string previewRoot = PreviewRootUtf8();
    const std::wstring known = KnownFolderLocalAppData();
    check(!local.empty(), "missing env still has local root");
    check(!roaming.empty(), "missing env still has roaming root");
    check(PathIsUnderUtf8(local, previewRoot), "missing LOCALAPPDATA stays under preview root");
    check(PathIsUnderUtf8(roaming, previewRoot), "missing APPDATA stays under preview root");
    check(roots.localAppData != known, "missing env does not fall back to KnownFolder");
    const auto layout = EnsureLayout();
    check(layout.kind == LayoutKind::FreshInstall, "missing env does not migrate production legacy");
    check(PathIsUnderUtf8(WideToUtf8Path(layout.canonicalRoot), previewRoot),
          "canonical root stays under preview root when env missing");
}

void TestOneEnvMissing() {
    const std::string previewRoot = PreviewRootUtf8();
    _wputenv_s(L"LOCALAPPDATA", L"");
    _wputenv_s(L"APPDATA", (fs::path(previewRoot) / "profile" / "Roaming").wstring().c_str());
    ResetLayoutCacheForTests();
    auto roots = ResolveRoots();
    check(PathIsUnderUtf8(WideToUtf8Path(roots.localAppData), previewRoot), "LOCALAPPDATA missing still pinned");
    check(PathIsUnderUtf8(WideToUtf8Path(roots.roamingAppData), previewRoot), "present APPDATA inside root kept or pinned");

    _wputenv_s(L"LOCALAPPDATA", (fs::path(previewRoot) / "profile" / "Local").wstring().c_str());
    _wputenv_s(L"APPDATA", L"");
    ResetLayoutCacheForTests();
    roots = ResolveRoots();
    check(PathIsUnderUtf8(WideToUtf8Path(roots.localAppData), previewRoot), "present LOCALAPPDATA inside root kept or pinned");
    check(PathIsUnderUtf8(WideToUtf8Path(roots.roamingAppData), previewRoot), "APPDATA missing still pinned");
}

void TestOutsideEnvRejected() {
    const std::string previewRoot = PreviewRootUtf8();
    const fs::path outsideLocal = fs::path(L"E:/XDGit/xindong/aris/.local/preview-original-ui-20260922-contract-outside/Local");
    const fs::path outsideRoaming = fs::path(L"E:/XDGit/xindong/aris/.local/preview-original-ui-20260922-contract-outside/Roaming");
    _wputenv_s(L"LOCALAPPDATA", outsideLocal.wstring().c_str());
    _wputenv_s(L"APPDATA", outsideRoaming.wstring().c_str());
    ResetLayoutCacheForTests();
    const auto roots = ResolveRoots();
    const std::string local = WideToUtf8Path(roots.localAppData);
    const std::string roaming = WideToUtf8Path(roots.roamingAppData);
    check(PathIsUnderUtf8(local, previewRoot), "outside LOCALAPPDATA is not used");
    check(PathIsUnderUtf8(roaming, previewRoot), "outside APPDATA is not used");
    check(local.find("preview-original-ui-20260922-contract-outside") == std::string::npos,
          "local root is not the fictional outside path");
    check(roaming.find("preview-original-ui-20260922-contract-outside") == std::string::npos,
          "roaming root is not the fictional outside path");
}

std::wstring FullPathWide(const std::wstring& value) {
    wchar_t buf[32768] = {};
    const DWORD n = GetFullPathNameW(value.c_str(), 32768, buf, nullptr);
    if (n == 0 || n >= 32768) return {};
    std::wstring out(buf);
    while (!out.empty() && (out.back() == L'\\' || out.back() == L'/')) out.pop_back();
    return out;
}

std::wstring ExpectedPinnedLocal() {
    return FullPathWide(Utf8ToWidePath(PreviewRootUtf8()) + L"\\profile\\Local");
}

std::wstring ExpectedPinnedRoaming() {
    return FullPathWide(Utf8ToWidePath(PreviewRootUtf8()) + L"\\profile\\Roaming");
}

void TestDotDotAndMixedSlashEscape() {
    const std::string previewRoot = PreviewRootUtf8();
    const std::string parentEscape = previewRoot + "/../outside.db";
    const std::string nestedEscapeTight = previewRoot + "/profile/Local/../../../outside.db";
    const std::string mixedSlash = previewRoot + "\\profile/Local/..\\..\\..\\outside.db";
    const std::string mixedSlash2 = previewRoot + "/profile\\Local/../..\\..\\outside.db";
    const std::string adjacentPrefix = previewRoot + "x/HarkenSearch/Database/preview.db";
    const std::string inside = previewRoot + "/profile/Local/HarkenSearch/Database/preview.db";
    const std::string sidecar = previewRoot + "/profile/Local/HarkenSearch/Database/preview.db-wal";
    const std::string outside = "E:/XDGit/xindong/aris/.local/preview-original-ui-20260922-contract-outside/preview.db";
    check(!PreviewIsolationAllowsDataPathUtf8(parentEscape), "root/../outside.db rejected");
    check(!PreviewIsolationAllowsDataPathUtf8(nestedEscapeTight), "nested .. escape rejected");
    check(!PreviewIsolationAllowsDataPathUtf8(mixedSlash), "mixed-slash .. escape rejected");
    check(!PreviewIsolationAllowsDataPathUtf8(mixedSlash2), "mixed-slash alt escape rejected");
    check(!PreviewIsolationAllowsDataPathUtf8(adjacentPrefix), "adjacent prefix path rejected");
    check(PreviewIsolationAllowsDataPathUtf8(inside), "canonical in-root db still allowed");
    check(PreviewIsolationAllowsDataPathUtf8(sidecar), "canonical in-root sidecar still allowed");
    check(!PreviewIsolationAllowsDataPathUtf8(outside), "literal outside db still rejected");
    check(!PreviewIsolationAllowsDataPathUtf8(""), "empty db path still rejected");
}

void TestMaliciousAppDataEnvIsPinned() {
    const std::string previewRoot = PreviewRootUtf8();
    const std::wstring pinnedLocal = ExpectedPinnedLocal();
    const std::wstring pinnedRoaming = ExpectedPinnedRoaming();
    const fs::path escapeLocal = fs::path(previewRoot) / "profile" / "Local" / ".." / ".." / "preview-original-ui-20260922-contract-outside" / "Local";
    const fs::path dotdotLocal = fs::path(previewRoot) / "profile" / "Local" / "..";
    _wputenv_s(L"LOCALAPPDATA", escapeLocal.wstring().c_str());
    _wputenv_s(L"APPDATA", (fs::path(previewRoot) / "profile" / "Roaming" / "..").wstring().c_str());
    ResetLayoutCacheForTests();
    auto roots = ResolveRoots();
    check(FullPathWide(roots.localAppData) == pinnedLocal, "malicious LOCALAPPDATA pins to exact Local");
    check(FullPathWide(roots.roamingAppData) == pinnedRoaming, "dotdot APPDATA pins to exact Roaming");
    check(WideToUtf8Path(roots.localAppData).find("..") == std::string::npos, "pinned local has no ..");
    check(WideToUtf8Path(roots.roamingAppData).find("..") == std::string::npos, "pinned roaming has no ..");
    check(WideToUtf8Path(roots.localAppData).find("contract-outside") == std::string::npos,
          "pinned local is not contract-outside");

    _wputenv_s(L"LOCALAPPDATA", dotdotLocal.wstring().c_str());
    _wputenv_s(L"APPDATA", L"");
    ResetLayoutCacheForTests();
    roots = ResolveRoots();
    check(FullPathWide(roots.localAppData) == pinnedLocal, "LOCALAPPDATA with .. pins to exact Local");
    check(FullPathWide(roots.roamingAppData) == pinnedRoaming, "missing APPDATA pins to exact Roaming");
}

bool CreateTestJunction(const std::wstring& link, const std::wstring& target) {
    SHCreateDirectoryExW(nullptr, target.c_str(), nullptr);
    RemoveDirectoryW(link.c_str());
    std::wstring cmd = L"cmd.exe /c mklink /J \"";
    cmd += link;
    cmd += L"\" \"";
    cmd += target;
    cmd += L"\"";
    return _wsystem(cmd.c_str()) == 0;
}

void TestReparseJunctionRejected() {
    const std::wstring sandbox = Utf8ToWidePath(PreviewRootUtf8()) + L"\\temp\\path-guard-r3";
    const std::wstring target = sandbox + L"\\real_target";
    const std::wstring junc = sandbox + L"\\junc";
    SHCreateDirectoryExW(nullptr, sandbox.c_str(), nullptr);
    const bool made = CreateTestJunction(junc, target);
    check(made, "synthetic junction created inside preview temp");
    const DWORD attr = GetFileAttributesW(junc.c_str());
    check(attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_REPARSE_POINT),
          "synthetic junction is a reparse point");
    const std::string juncDb = WideToUtf8Path(junc + L"\\preview.db");
    check(!PreviewIsolationAllowsDataPathUtf8(juncDb), "path through junction is rejected");
    RemoveDirectoryW(junc.c_str());
    RemoveDirectoryW(target.c_str());
}


void TestNewRootIdentityAndOldPreviewRejected() {
    const std::string previewRoot = PreviewRootUtf8();
    check(!previewRoot.empty() && previewRoot == ARIS_PREVIEW_ISOLATION_ROOT_UTF8,
          "compile-time isolation root is the configured synthetic preview");
    check(previewRoot.find("preview-original-ui-20260922-7450391cc") == std::string::npos,
          "compile-time isolation root is not the old 229A preview");
    const std::string oldDb =
        "E:/XDGit/xindong/aris/.local/preview-original-ui-20260922-7450391cc/profile/Local/HarkenSearch/Database/Harken Search/main.db";
    check(!PreviewIsolationAllowsDataPathUtf8(oldDb), "old 229A main.db path is rejected as outside the new root");
}

int main() {
    TestNewRootIdentityAndOldPreviewRejected();
    TestCompileIsolationStaysOnWithoutEnv();
    TestMissingEnvDoesNotUseKnownFolder();
    TestOneEnvMissing();
    TestOutsideEnvRejected();
    TestDotDotAndMixedSlashEscape();
    TestMaliciousAppDataEnvIsPinned();
    TestReparseJunctionRejected();
    if (g_failures == 0) std::printf("all preview isolation guard checks pass\n");
    return g_failures == 0 ? 0 : 1;
}

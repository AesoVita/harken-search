#include "Utils.h"
#include "platform/harken_user_data_layout.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace fs = std::filesystem;

std::string ResolveCanonicalHarkenSearchDatabasePathUtf8(const std::string& pathUtf8);

int main() {
    const fs::path root = fs::temp_directory_path() /
        (L"harken-flat-db-path-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
         std::to_wstring(GetTickCount64()));
    const fs::path local = root / L"Local";
    const fs::path roaming = root / L"Roaming";
    const fs::path oldDb = local / L"Auro" / L"auro.db";
    fs::create_directories(oldDb.parent_path() / L"Database");
    fs::create_directories(roaming);
    { std::ofstream out(oldDb, std::ios::binary); out << "legacy-db"; }
    { std::ofstream out(local / L"HarkenSearch", std::ios::binary); out << "blocked"; }
    if (_wputenv_s(L"LOCALAPPDATA", local.c_str()) != 0 ||
        _wputenv_s(L"APPDATA", roaming.c_str()) != 0) return 2;
    Platform::HarkenUserData::ResetLayoutCacheForTests();

    const auto layout = Platform::HarkenUserData::EnsureLayout();
    const std::string expected = Platform::HarkenUserData::WideToUtf8Path(oldDb.wstring());
    const bool ok = layout.readOnly &&
        layout.kind == Platform::HarkenUserData::LayoutKind::FallbackLegacy &&
        GetDefaultDbPathUtf8() == expected &&
        ResolveCanonicalHarkenSearchDatabasePathUtf8(expected) == expected &&
        !fs::exists(oldDb.parent_path() / L"Auro" / L"main.db");
    if (!ok) std::cerr << "FAIL: read-only fallback remapped the existing flat DB\n";
    return ok ? 0 : 1;
}

#include "platform/theme_json_dialogs.h"
#include "Localization.h"
#include <string>

#include "platform/text_encoding.h"
#include "platform/text_encoding.inl"

#include <array>
#include <filesystem>

#include <commdlg.h>
#include <windows.h>

namespace Platform::ThemeDialogs {
namespace {
using namespace std::string_literals;

std::wstring ThemeJsonFilter() {
    // Runtime-localized filter (ledger display names + in-code patterns);
    // Each dialog owns the buffer so language changes take effect immediately.
    const std::wstring filter =
        std::wstring(Lw(Dlg_FilterThemeJson)) + L"\0*.json\0"s +
        std::wstring(Lw(Dlg_FilterJsonFiles)) + L"\0*.json\0"s +
        std::wstring(Lw(Dlg_FilterAllFiles)) + L"\0*.*\0"s;
    return filter;
}

std::wstring InitialDirFor(const std::filesystem::path& path) {
    return path.has_parent_path() ? path.parent_path().wstring() : std::wstring();
}

void CopyInitialName(std::array<wchar_t, MAX_PATH>& fileBuffer, const std::filesystem::path& path) {
    const std::wstring initialName = path.filename().wstring();
    wcsncpy_s(fileBuffer.data(), fileBuffer.size(), initialName.c_str(), _TRUNCATE);
}

} // namespace

std::string OpenThemeJsonFileDialogUtf8(void* ownerWindow, const std::string& suggestedPathUtf8) {
    OPENFILENAMEW ofn = {};
    std::array<wchar_t, MAX_PATH> fileBuffer{};
    const std::filesystem::path fallbackPath(TextEncoding::Utf8ToWide(suggestedPathUtf8));
    const std::wstring initialDir = InitialDirFor(fallbackPath);
    CopyInitialName(fileBuffer, fallbackPath);

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = static_cast<HWND>(ownerWindow);
    const std::wstring filterBuf = ThemeJsonFilter();
    ofn.lpstrFilter = filterBuf.c_str();
    ofn.lpstrFile = fileBuffer.data();
    ofn.nMaxFile = static_cast<DWORD>(fileBuffer.size());
    ofn.lpstrInitialDir = initialDir.empty() ? nullptr : initialDir.c_str();
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = L"json";
    const std::wstring titleBuf = Lw(Dlg_TitleImportTheme);

    ofn.lpstrTitle = titleBuf.c_str();
    if (!GetOpenFileNameW(&ofn)) return "";
    return TextEncoding::WideToUtf8(std::wstring(fileBuffer.data()));
}

std::string SaveThemeJsonFileDialogUtf8(void* ownerWindow, const std::string& suggestedPathUtf8) {
    OPENFILENAMEW ofn = {};
    std::array<wchar_t, MAX_PATH> fileBuffer{};
    const std::filesystem::path fallbackPath(TextEncoding::Utf8ToWide(suggestedPathUtf8));
    const std::wstring initialDir = InitialDirFor(fallbackPath);
    CopyInitialName(fileBuffer, fallbackPath);

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = static_cast<HWND>(ownerWindow);
    const std::wstring filterBuf = ThemeJsonFilter();
    ofn.lpstrFilter = filterBuf.c_str();
    ofn.lpstrFile = fileBuffer.data();
    ofn.nMaxFile = static_cast<DWORD>(fileBuffer.size());
    ofn.lpstrInitialDir = initialDir.empty() ? nullptr : initialDir.c_str();
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = L"json";
    const std::wstring titleBuf = Lw(Dlg_TitleExportTheme);

    ofn.lpstrTitle = titleBuf.c_str();
    if (!GetSaveFileNameW(&ofn)) return "";
    return TextEncoding::WideToUtf8(std::wstring(fileBuffer.data()));
}

} // namespace Platform::ThemeDialogs

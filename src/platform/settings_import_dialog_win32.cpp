#include "platform/settings_import_dialog.h"
#include "Localization.h"
#include <string>

using namespace std::string_literals;

#include "platform/text_encoding.h"
#include "platform/text_encoding.inl"

#include <commdlg.h>
#include <windows.h>

namespace Platform::SettingsDialogs {

std::string OpenSettingsImportFileDialogUtf8(void* ownerWindow) {
    OPENFILENAMEW ofn = {};
    wchar_t filePath[MAX_PATH] = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = static_cast<HWND>(ownerWindow);
    const std::wstring filterBuf = (std::wstring(Lw(Dlg_FilterSettings)) + L"\0*.ini\0"s + std::wstring(Lw(Dlg_FilterAllFilesPat)) + L"\0*.*\0"s);
    ofn.lpstrFilter = filterBuf.c_str();
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&ofn)) return "";
    return TextEncoding::WideToUtf8(std::wstring(filePath));
}

} // namespace Platform::SettingsDialogs

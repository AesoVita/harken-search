#include "platform/database_file_dialog.h"
#include "Localization.h"
#include <string>

using namespace std::string_literals;

#include "platform/text_encoding.h"
#include "platform/text_encoding.inl"

#include <commdlg.h>
#include <windows.h>

namespace Platform::DatabaseDialogs {

std::string OpenDatabaseFileDialogUtf8(void* ownerWindow) {
    OPENFILENAMEW ofn = {};
    wchar_t szFileName[MAX_PATH] = L"";
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = static_cast<HWND>(ownerWindow);
    const std::wstring filterBuf = (std::wstring(Lw(Dlg_FilterAuroDb)) + L"\0*.db\0"s + std::wstring(Lw(Dlg_FilterAllFiles)) + L"\0*.*\0"s);
    ofn.lpstrFilter = filterBuf.c_str();    ofn.lpstrFile = szFileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = L"db";
    const std::wstring titleBuf = Lw(Dlg_TitleOpenDatabase);

    ofn.lpstrTitle = titleBuf.c_str();
    if (!GetOpenFileNameW(&ofn)) return "";
    return TextEncoding::WideToUtf8(std::wstring(szFileName));
}

std::string OpenDatabaseFileDialogWithInitialDirectoryUtf8(
    void* ownerWindow,
    const std::wstring& initialDirectory) {
    OPENFILENAMEW ofn = {};
    wchar_t szFileName[MAX_PATH] = L"";
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = static_cast<HWND>(ownerWindow);
    const std::wstring filterBuf = (std::wstring(Lw(Dlg_FilterArisDb)) + L"\0*.db\0"s + std::wstring(Lw(Dlg_FilterAllFiles)) + L"\0*.*\0"s);
    ofn.lpstrFilter = filterBuf.c_str();    ofn.lpstrFile = szFileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = initialDirectory.c_str();
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = L"db";
    const std::wstring titleBuf = Lw(Dlg_TitleOpenDatabase);

    ofn.lpstrTitle = titleBuf.c_str();
    if (!GetOpenFileNameW(&ofn)) return "";
    return TextEncoding::WideToUtf8(std::wstring(szFileName));
}

} // namespace Platform::DatabaseDialogs

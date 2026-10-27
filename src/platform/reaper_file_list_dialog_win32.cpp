#include "platform/reaper_file_list_dialog.h"
#include "Localization.h"
#include <string>

using namespace std::string_literals;

#include "platform/text_encoding.h"
#include "platform/text_encoding.inl"

#include <commdlg.h>
#include <windows.h>

namespace Platform::ReaperDialogs {

std::string OpenReaperFileListDialogUtf8(void* ownerWindow) {
    OPENFILENAMEW ofn = {};
    wchar_t szFileName[MAX_PATH] = L"";
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = static_cast<HWND>(ownerWindow);
    const std::wstring filterBuf = (std::wstring(Lw(Dlg_FilterReaperList)) + L"\0*.ReaperFileList\0"s + std::wstring(Lw(Dlg_FilterAllFilesPat)) + L"\0*.*\0"s);
    ofn.lpstrFilter = filterBuf.c_str();
    ofn.lpstrFile = szFileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = L"ReaperFileList";
    const std::wstring titleBuf = Lw(Dlg_TitleImportReaperList);

    ofn.lpstrTitle = titleBuf.c_str();
    if (!GetOpenFileNameW(&ofn)) return "";
    return TextEncoding::WideToUtf8(std::wstring(szFileName));
}

} // namespace Platform::ReaperDialogs

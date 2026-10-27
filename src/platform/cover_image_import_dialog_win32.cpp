#include "platform/cover_image_import_dialog.h"
#include "Localization.h"
#include <string>

using namespace std::string_literals;

#include "platform/text_encoding.h"
#include "platform/text_encoding.inl"

#include <commdlg.h>
#include <windows.h>

namespace Platform::CoverDialogs {

std::string OpenCoverImageImportFileDialogUtf8(void* ownerWindow) {
    wchar_t szFile[260] = {0};
    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = static_cast<HWND>(ownerWindow);
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(szFile[0]);
    const std::wstring filterBuf = (std::wstring(Lw(Dlg_FilterImageFiles)) + L"\0*.jpg;*.jpeg;*.png\0"s + std::wstring(Lw(Dlg_FilterJpegFiles)) + L"\0*.jpg;*.jpeg\0"s + std::wstring(Lw(Dlg_FilterPngFiles)) + L"\0*.png\0"s + std::wstring(Lw(Dlg_FilterAllFiles)) + L"\0*.*\0"s);
    ofn.lpstrFilter = filterBuf.c_str();
    ofn.nFilterIndex = 1;
    const std::wstring titleBuf = Lw(Dlg_TitleChooseCover);

    ofn.lpstrTitle = titleBuf.c_str();
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) return "";
    return TextEncoding::WideToUtf8(std::wstring(szFile));
}

} // namespace Platform::CoverDialogs

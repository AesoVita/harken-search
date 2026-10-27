#include "platform/executable_file_dialog.h"
#include "Localization.h"
#include <string>

using namespace std::string_literals;

#include "platform/text_encoding.h"
#include "platform/text_encoding.inl"

#include <commdlg.h>
#include <windows.h>

namespace Platform::ExecutableDialogs {

std::string OpenExecutableFileDialogUtf8(
    void* ownerWindow,
    const std::wstring& title,
    const std::wstring& initialPath) {
    OPENFILENAMEW ofn = {};
    wchar_t szFileName[MAX_PATH] = L"";
    std::wstring initialDir;
    if (!initialPath.empty()) {
        std::wstring normalized = initialPath;
        size_t slashPos = normalized.find_last_of(L"\\/");
        if (slashPos != std::wstring::npos) {
            initialDir = normalized.substr(0, slashPos);
        }
        wcsncpy_s(szFileName, initialPath.c_str(), _TRUNCATE);
    }
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = static_cast<HWND>(ownerWindow);
    const std::wstring filterBuf = (std::wstring(Lw(Dlg_FilterExecutable)) + L"\0*.exe\0"s + std::wstring(Lw(Dlg_FilterAllFilesPat)) + L"\0*.*\0"s);
    ofn.lpstrFilter = filterBuf.c_str();
    ofn.lpstrFile = szFileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = initialDir.empty() ? nullptr : initialDir.c_str();
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = L"exe";
    ofn.lpstrTitle = title.c_str();
    if (!GetOpenFileNameW(&ofn)) return "";
    return TextEncoding::WideToUtf8(std::wstring(szFileName));
}

} // namespace Platform::ExecutableDialogs

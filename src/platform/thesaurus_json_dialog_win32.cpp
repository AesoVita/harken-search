#include "platform/thesaurus_json_dialog.h"

#include "platform/text_encoding.h"
#include "platform/text_encoding.inl"

#include <commdlg.h>
#include <windows.h>

namespace Platform::ThesaurusDialogs {

std::string OpenThesaurusJsonFileDialogUtf8(void* ownerWindow) {
    OPENFILENAMEW ofn = {};
    wchar_t szFileName[MAX_PATH] = L"";
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = static_cast<HWND>(ownerWindow);
    ofn.lpstrFilter = L"JSON Files\0*.json\0All Files\0*.*\0";
    ofn.lpstrFile = szFileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) return "";
    return TextEncoding::WideToUtf8(std::wstring(szFileName));
}

} // namespace Platform::ThesaurusDialogs

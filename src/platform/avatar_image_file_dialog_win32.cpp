#include "platform/avatar_image_file_dialog.h"
#include "Localization.h"
#include <string>

using namespace std::string_literals;

#include "platform/text_encoding.h"
#include "platform/text_encoding.inl"

#include <commdlg.h>
#include <windows.h>

namespace Platform::AvatarDialogs {

std::string OpenAvatarImageFileDialogUtf8(void* ownerWindow) {
    OPENFILENAMEW ofn = {};
    wchar_t szFileName[MAX_PATH] = L"";
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = static_cast<HWND>(ownerWindow);
    const std::wstring filterBuf = (std::wstring(Lw(Dlg_FilterImageFiles)) + L"\0*.png;*.jpg;*.jpeg;*.bmp;*.tga\0"s + std::wstring(Lw(Dlg_FilterAllFiles)) + L"\0*.*\0"s);
    ofn.lpstrFilter = filterBuf.c_str();
    ofn.lpstrFile = szFileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&ofn)) return "";
    return TextEncoding::WideToUtf8(std::wstring(szFileName));
}

} // namespace Platform::AvatarDialogs

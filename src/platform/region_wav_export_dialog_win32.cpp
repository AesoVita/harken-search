#include "platform/region_wav_export_dialog.h"
#include "Localization.h"
#include <string>

using namespace std::string_literals;

#include "platform/text_encoding.h"
#include "platform/text_encoding.inl"

#include <commdlg.h>
#include <windows.h>

namespace Platform::RegionDialogs {

std::string SaveRegionWavFileDialogUtf8(void* ownerWindow) {
    OPENFILENAMEW ofn = {};
    wchar_t szFileName[MAX_PATH] = L"region_export.wav";
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = static_cast<HWND>(ownerWindow);
    const std::wstring filterBuf = (std::wstring(Lw(Dlg_FilterWav)) + L"\0*.wav\0"s + std::wstring(Lw(Dlg_FilterAllFiles)) + L"\0*.*\0"s);
    ofn.lpstrFilter = filterBuf.c_str();
    ofn.lpstrFile = szFileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt = L"wav";
    const std::wstring titleBuf = Lw(Dlg_TitleExportRegion);

    ofn.lpstrTitle = titleBuf.c_str();

    if (!GetSaveFileNameW(&ofn)) return "";
    return TextEncoding::WideToUtf8(std::wstring(szFileName));
}

} // namespace Platform::RegionDialogs

#include "platform/floating_window_corners.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace Platform {

// Window properties expire with the window, including on handle reuse.
bool ApplyNativeFloatingWindowCorners(void* nativeWindow, int radius) {
    HWND hwnd = static_cast<HWND>(nativeWindow);
    RECT bounds{};
    if (!hwnd || radius <= 0 || !::GetWindowRect(hwnd, &bounds)) return false;
    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;
    if (width <= radius * 2 || height <= radius * 2) return false;

    constexpr wchar_t kWidth[] = L"Aris.ModuleShell.RoundWidth";
    constexpr wchar_t kHeight[] = L"Aris.ModuleShell.RoundHeight";
    constexpr wchar_t kRadius[] = L"Aris.ModuleShell.RoundRadius";
    const HANDLE widthStamp = reinterpret_cast<HANDLE>(static_cast<INT_PTR>(width));
    const HANDLE heightStamp = reinterpret_cast<HANDLE>(static_cast<INT_PTR>(height));
    const HANDLE radiusStamp = reinterpret_cast<HANDLE>(static_cast<INT_PTR>(radius));
    if (::GetPropW(hwnd, kWidth) == widthStamp &&
        ::GetPropW(hwnd, kHeight) == heightStamp &&
        ::GetPropW(hwnd, kRadius) == radiusStamp) return true;

    HRGN region = ::CreateRoundRectRgn(0, 0, width + 1, height + 1,
                                     radius * 2, radius * 2);
    if (!region) return false;
    if (!::SetWindowRgn(hwnd, region, TRUE)) {
        ::DeleteObject(region);
        return false;
    }
    // Windows owns region after SetWindowRgn succeeds. Reapply only on resize
    // (or a changed radius), including resize of an existing native viewport.
    ::SetPropW(hwnd, kWidth, widthStamp);
    ::SetPropW(hwnd, kHeight, heightStamp);
    ::SetPropW(hwnd, kRadius, radiusStamp);
    return true;
}

} // namespace Platform

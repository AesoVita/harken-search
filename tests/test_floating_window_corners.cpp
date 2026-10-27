// Include the public interface first so it must be self-contained.
#include "platform/floating_window_corners.h"
#include <windows.h>
#include <cassert>

int main() {
    assert(!Platform::ApplyNativeFloatingWindowCorners(nullptr, 9));
    HWND window = ::CreateWindowExW(0, L"STATIC", L"corner-test", WS_POPUP,
                                    0, 0, 160, 100, nullptr, nullptr, nullptr, nullptr);
    assert(window);
    assert(!::IsWindowVisible(window));
    assert(!Platform::ApplyNativeFloatingWindowCorners(window, 0));
    assert(!Platform::ApplyNativeFloatingWindowCorners(window, 60));
    assert(Platform::ApplyNativeFloatingWindowCorners(window, 9));
    HRGN first = ::CreateRectRgn(0, 0, 0, 0);
    HRGN second = ::CreateRectRgn(0, 0, 0, 0);
    assert(first && second);
    assert(::GetWindowRgn(window, first) != ERROR);
    assert(!::PtInRegion(first, 0, 0));
    assert(::PtInRegion(first, 80, 50));
    assert(Platform::ApplyNativeFloatingWindowCorners(window, 9));
    assert(::GetWindowRgn(window, second) != ERROR);
    assert(::EqualRgn(first, second));
    assert(::SetWindowPos(window, nullptr, 0, 0, 220, 140, SWP_NOZORDER | SWP_NOMOVE));
    assert(Platform::ApplyNativeFloatingWindowCorners(window, 9));
    assert(::GetWindowRgn(window, second) != ERROR);
    assert(!::EqualRgn(first, second));
    assert(::PtInRegion(second, 200, 100));
    assert(Platform::ApplyNativeFloatingWindowCorners(window, 16));
    assert(::GetWindowRgn(window, first) != ERROR);
    assert(!::EqualRgn(first, second));
    assert(::DeleteObject(first));
    assert(::DeleteObject(second));
    assert(::DestroyWindow(window));
    return 0;
}

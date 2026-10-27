"""Keep native window-region implementation out of the portable interface."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def test_floating_corners_public_header_does_not_expose_windows_types():
    header = (ROOT / "src/platform/floating_window_corners.h").read_text(encoding="utf-8")
    assert "bool ApplyNativeFloatingWindowCorners(void* nativeWindow, int radius);" in header
    for native_detail in ("windows.h", "HWND", "HANDLE", "HRGN", "RECT", "SetWindowRgn"):
        assert native_detail not in header


def test_floating_corners_windows_implementation_is_registered():
    source = ROOT / "src/platform/floating_window_corners_win32.cpp"
    assert source.is_file()
    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    platform_block = cmake.split("# Platform abstraction layer (F-031)", 1)[1].split("elseif(APPLE)", 1)[0]
    assert "src/platform/floating_window_corners_win32.cpp" in platform_block

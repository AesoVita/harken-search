from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = (ROOT / "src" / "WindowsMain.h").read_text(encoding="utf-8")


def test_windows_main_header_does_not_pull_win32_sdk_into_consumers():
    assert "#include <windows.h>" not in HEADER
    assert "using HWND = HWND__*;" in HEADER
    assert "using WINDOWPLACEMENT = tagWINDOWPLACEMENT;" in HEADER
    assert "using WPARAM = std::uintptr_t;" in HEADER


def test_windows_main_header_keeps_message_ids_sdk_independent():
    assert "WM_APP +" not in HEADER
    assert "0x8000L + 42" in HEADER

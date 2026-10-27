from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def test_temp_drag_directory_is_owned_by_system_pal():
    main = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
    header = (ROOT / "src" / "platform" / "system.h").read_text(encoding="utf-8")
    win32 = (ROOT / "src" / "platform" / "system_win32.cpp").read_text(encoding="utf-8")
    macos = (ROOT / "src" / "platform" / "system_macos.mm").read_text(encoding="utf-8")

    assert "std::string GetTempDragDirectory();" in header
    helper = main.split("static std::string GetTempDragDirUtf8()", 1)[1].split("}", 1)[0]
    assert "Platform::GetTempDragDirectory()" in helper
    assert "GetTempPathW" not in helper
    assert "CreateDirectoryW" not in helper
    assert "std::string GetTempDragDirectory()" in win32
    assert "std::string GetTempDragDirectory()" in macos

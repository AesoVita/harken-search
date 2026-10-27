import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
HEADER_PATH = ROOT / "src" / "PlaylistQuickBar.h"
SOURCE_PATH = ROOT / "src" / "PlaylistQuickBar.cpp"
SCROLL_HEADER = ROOT / "src" / "TransientScrollbarHost.h"
SCROLL_SOURCE = ROOT / "src" / "TransientScrollbarHost.cpp"
CMAKE = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
BEHAVIOR = (ROOT / "cmake" / "harken_search_behavior_tests.cmake").read_text(
    encoding="utf-8"
)
RULES = (
    ROOT / "docs" / "architecture" / "stage-4-dependency-rules.json"
).read_text(encoding="utf-8")
STAGE7 = (ROOT / "tools" / "stage7_api_contract_audit.py").read_text(
    encoding="utf-8"
)


def test_playlist_quick_bar_files_own_drawing_entry():
    assert HEADER_PATH.exists()
    assert SOURCE_PATH.exists()
    assert SCROLL_HEADER.exists()
    assert SCROLL_SOURCE.exists()

    header = HEADER_PATH.read_text(encoding="utf-8")
    source = SOURCE_PATH.read_text(encoding="utf-8")
    scroll_header = SCROLL_HEADER.read_text(encoding="utf-8")
    scroll_source = SCROLL_SOURCE.read_text(encoding="utf-8")

    assert "void RenderPlaylistQuickBar(AppState& state, float height);" in header
    assert "void RenderPlaylistQuickBarRestoreStrip(AppState& state, float height);" in header
    assert "void RenderPlaylistQuickBar(AppState& state, float height)" in source
    assert "void RenderPlaylistQuickBarRestoreStrip(AppState& state, float height)" in source
    assert "BeginTransientScrollbarChild" in source
    assert "bool BeginTransientScrollbarChild(" in scroll_header
    assert "transientScrollbarController" in scroll_source

    for forbidden in ("main.cpp", "WindowsMain", "platform/"):
        assert forbidden not in header
        assert forbidden not in source
        assert forbidden not in scroll_header
        assert forbidden not in scroll_source


def test_root_delegates_playlist_quick_bar_without_owning_drawing():
    assert '#include "PlaylistQuickBar.h"' in MAIN
    assert '#include "TransientScrollbarHost.h"' in MAIN
    assert "static void RenderPlaylistQuickBar(AppState& state, float height)" not in MAIN
    assert "static void RenderPlaylistQuickBarRestoreStrip(AppState& state, float height)" not in MAIN
    assert "static bool BeginTransientScrollbarChild(" not in MAIN
    assert "RenderPlaylistQuickBar(state, waveQuickBarH);" in MAIN
    assert "int64_t CopyPlaylistFromUi(AppState& state, const Playlist& playlist)" in MAIN
    assert "bool OpenPlaylistRenameDialog(AppState& state, const Playlist& playlist)" in MAIN
    assert "void RenderPlaylistQuickBar(AppState& state, float height)" not in MAIN


def test_playlist_quick_bar_is_built_tested_and_governed():
    assert CMAKE.count("src/PlaylistQuickBar.cpp") == 1
    assert CMAKE.count("src/TransientScrollbarHost.cpp") == 1
    assert BEHAVIOR.count("tests/test_playlist_quick_bar_boundary.py") == 1
    assert RULES.count("src/PlaylistQuickBar.cpp") >= 2
    assert RULES.count("src/PlaylistQuickBar.h") >= 2
    assert RULES.count("src/TransientScrollbarHost.cpp") >= 2
    assert RULES.count("src/TransientScrollbarHost.h") >= 2
    modules = json.loads(RULES)["modules"]
    for path in ("src/PlaylistQuickBar.cpp", "src/PlaylistQuickBar.h"):
        assert path in modules["ui_orchestration"]["files"]
        assert path not in modules["domain_policy"]["files"]
    assert (
        '"src/PlaylistQuickBar.h": '
        '"tests/test_playlist_quick_bar_boundary.py"'
        in STAGE7
    )
    assert (
        '"src/TransientScrollbarHost.h": '
        '"tests/test_playlist_quick_bar_boundary.py"'
        in STAGE7
    )


def test_extracted_units_see_needed_symbols_without_main_statics():
    source = SOURCE_PATH.read_text(encoding="utf-8")
    header = HEADER_PATH.read_text(encoding="utf-8")
    scroll_source = SCROLL_SOURCE.read_text(encoding="utf-8")
    main = MAIN

    assert '#include "DiagnosticsLogger.h"' in source
    assert '#include "WindowsMain.h"' not in source
    assert "struct ReadingTextScope" in source
    assert "EnsurePlaylistQuickBarAnimItem" in header
    assert "PrunePlaylistQuickBarAnimItems" in header
    assert "g_playlistQuickBarReorderAnim" in header
    assert "extern PlaylistQuickBarReorderAnimState g_playlistQuickBarReorderAnim;" in header
    assert "static PlaylistQuickBarReorderAnimState g_playlistQuickBarReorderAnim;" not in main
    assert "static PlaylistQuickBarReorderAnimState::Item& EnsurePlaylistQuickBarAnimItem(" not in main
    assert "static void PrunePlaylistQuickBarAnimItems(" not in main
    assert "PlaylistQuickBarReorderAnimState g_playlistQuickBarReorderAnim;" in source
    assert "EnsurePlaylistQuickBarAnimItem(" in source
    assert "PrunePlaylistQuickBarAnimItems(" in source
    assert '#include "imgui_internal.h"' in scroll_source


if __name__ == "__main__":
    test_playlist_quick_bar_files_own_drawing_entry()
    test_root_delegates_playlist_quick_bar_without_owning_drawing()
    test_playlist_quick_bar_is_built_tested_and_governed()
    test_extracted_units_see_needed_symbols_without_main_statics()

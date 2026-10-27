"""The product uses one custom Dock tree; retained remote adapters stay inactive."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def test_only_the_active_custom_dock_sources_and_tests_remain():
    dock_dir = ROOT / "src" / "ui" / "dock"
    active_sources = {"DockCaptionWndProc.h", "DockPanelStateBridge.h"}
    present_sources = {p.name for p in dock_dir.iterdir() if p.suffix in {".cpp", ".h"}}
    assert present_sources == active_sources

    retired_tests = {
        "test_dock_controller.cpp", "test_dock_drag_commit.cpp",
        "test_dock_drag_wire.cpp", "test_dock_float_window.cpp",
        "test_dock_geometry_hit.cpp", "test_dock_model.cpp",
        "test_dock_module_rects.cpp", "test_dock_title_drag.cpp",
    }
    assert not retired_tests.intersection(p.name for p in (ROOT / "tests").iterdir())


def test_product_build_selects_only_the_custom_dock_tree():
    cmake = read("CMakeLists.txt")
    for source in ("src/DockLayout.cpp", "src/DockDragSession.cpp"):
        assert source in cmake
    for retired_runtime in ("DockImGuiAdapter.cpp", "DockModuleAdapters.cpp"):
        assert retired_runtime not in cmake
    for product in ("src/main.cpp", "src/WindowsMain.cpp"):
        assert "DockImGuiAdapter" not in read(product)
        assert "DockModuleAdapters" not in read(product)


def test_live_tree_owns_drag_commit_and_visible_geometry():
    main = read("src/main.cpp")
    hit = read("src/DockHitTest.h")
    assert "DockLayout g_dockLayout" in main
    assert "DockDragSession g_dockDrag(g_dockLayout)" in main
    assert "g_dockDrag.CommitOnMouseUp()" in main
    assert "BuildDockFrameGeometry(" in main
    assert "HitDockTarget(" in main
    assert "CollectLeafRects(" in hit
    assert "CollectAdjacentLeafGrips(" in hit


def test_test_controls_cannot_enter_native_dock_or_saved_layout():
    main = read("src/main.cpp")
    start = main.index("if (g_dockTestWindowOpen) {")
    control = main[start:main.index("// STATUS BAR", start)]
    assert "ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings" in control
    assert "ResetDockTestLayout();" in control
    assert "CollectLegalDockHints(" in main
    assert "g_dockTestBandHintsVisible = !g_dockTestBandHintsVisible" in control


def test_dock_state_bridge_does_not_own_user_data_or_native_windows():
    bridge = read("src/ui/dock/DockPanelStateBridge.h")
    for forbidden in ("HWND", "sqlite", "credential", "password", "audio file"):
        assert forbidden not in bridge

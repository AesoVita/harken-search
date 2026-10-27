from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "src/main.cpp"
HEADER = ROOT / "src/DeferredSidebarBootstrapWorkflow.h"
SOURCE = ROOT / "src/DeferredSidebarBootstrapWorkflow.cpp"
CMODEL = ROOT / "CMakeLists.txt"
BEHAVIOR = ROOT / "cmake/harken_search_behavior_tests.cmake"


def test_runtime_owns_start_pump_and_loading_state():
    main = MAIN.read_text(encoding="utf-8")
    header = HEADER.read_text(encoding="utf-8")
    source = SOURCE.read_text(encoding="utf-8")
    cmake = CMODEL.read_text(encoding="utf-8")
    behavior = BEHAVIOR.read_text(encoding="utf-8")

    assert "g_deferredSidebarBootstrap" not in main
    assert "void StartDeferredSidebarBootstrap" not in main
    assert "PumpDeferredSidebarBootstrap" not in main
    assert "StartDeferredSidebarBootstrap" in header
    assert "StartDeferredSidebarBootstrap(" in source
    assert "DeferredSidebarBootstrapWorkflow.cpp" in cmake
    assert "DeferredSidebarBootstrapWorkflow.cpp" in behavior


def test_runtime_has_no_root_platform_or_ui_dependency():
    source = SOURCE.read_text(encoding="utf-8")
    assert '"main.cpp"' not in source
    assert '"WindowsMain.h"' not in source
    assert "ImGui::" not in source

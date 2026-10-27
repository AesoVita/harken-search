from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "src/main.cpp"
HEADER = ROOT / "src/SingleChannelExportWorkflow.h"
SOURCE = ROOT / "src/SingleChannelExportWorkflow.cpp"
CMODEL = ROOT / "CMakeLists.txt"
BEHAVIOR = ROOT / "cmake/harken_search_behavior_tests.cmake"


def test_workflow_owns_single_channel_job_boundary():
    main = MAIN.read_text(encoding="utf-8")
    header = HEADER.read_text(encoding="utf-8")
    source = SOURCE.read_text(encoding="utf-8")
    cmake = CMODEL.read_text(encoding="utf-8")
    behavior = BEHAVIOR.read_text(encoding="utf-8")

    assert "RunSingleChannelExportJob" not in main
    assert "RunSingleChannelExport" in header
    assert "RunSingleChannelExport(" in source
    assert "SingleChannelExportWorkflow.cpp" in cmake
    assert "SingleChannelExportWorkflow.cpp" in behavior


def test_workflow_has_no_root_or_platform_entrypoint_dependency():
    source = SOURCE.read_text(encoding="utf-8")
    assert '"main.cpp"' not in source
    assert '"WindowsMain.h"' not in source
    assert "ImGui::" not in source


def test_focused_export_target_excludes_cover_cache_persistence():
    behavior = BEHAVIOR.read_text(encoding="utf-8")
    target_sources = behavior.split(
        "add_executable(TestSingleChannelExport EXCLUDE_FROM_ALL", 1
    )[1].split(
        "target_include_directories(TestSingleChannelExport", 1
    )[0]

    assert "src/CoverKeyCachePersistenceWorkflow.cpp" not in target_sources

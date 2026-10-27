from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "src/main.cpp"
HEADER = ROOT / "src/SingleChannelExportCompletionWorkflow.h"
SOURCE = ROOT / "src/SingleChannelExportCompletionWorkflow.cpp"
CMODEL = ROOT / "CMakeLists.txt"
BEHAVIOR = ROOT / "cmake/harken_search_behavior_tests.cmake"


def test_completion_workflow_owns_future_polling_boundary():
    main = MAIN.read_text(encoding="utf-8")
    header = HEADER.read_text(encoding="utf-8")
    source = SOURCE.read_text(encoding="utf-8")
    cmake = CMODEL.read_text(encoding="utf-8")
    behavior = BEHAVIOR.read_text(encoding="utf-8")

    assert "PumpSingleChannelExportJobs" not in main
    assert "PollChannelExportCompletions" in header
    assert "PollChannelExportCompletions(" in source
    assert "SingleChannelExportCompletionWorkflow.cpp" in cmake
    assert "SingleChannelExportCompletionWorkflow.cpp" in behavior


def test_completion_workflow_has_no_ui_or_platform_entrypoint_dependency():
    source = SOURCE.read_text(encoding="utf-8")
    assert '"main.cpp"' not in source
    assert '"WindowsMain.h"' not in source
    assert "ImGui::" not in source

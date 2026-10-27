from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "src/main.cpp"
HEADER = ROOT / "src/CoverKeyCachePersistenceWorkflow.h"
SOURCE = ROOT / "src/CoverKeyCachePersistenceWorkflow.cpp"
CMODEL = ROOT / "CMakeLists.txt"
BEHAVIOR = ROOT / "cmake/harken_search_behavior_tests.cmake"


def test_cover_key_persistence_owns_database_sync_boundary():
    main = MAIN.read_text(encoding="utf-8")
    header = HEADER.read_text(encoding="utf-8")
    source = SOURCE.read_text(encoding="utf-8")
    cmake = CMODEL.read_text(encoding="utf-8")
    behavior = BEHAVIOR.read_text(encoding="utf-8")

    assert "PumpCoverKeyCachePersistence" not in main
    assert "PumpPersistence" in header
    assert "PumpPersistence(" in source
    assert "CoverKeyCachePersistenceWorkflow.cpp" in cmake
    assert "TestCoverKeyCachePersistenceWorkflowBoundary" in behavior


def test_cover_key_persistence_has_no_root_or_ui_dependency():
    source = SOURCE.read_text(encoding="utf-8")
    assert '"main.cpp"' not in source
    assert '"WindowsMain.h"' not in source
    assert "ImGui::" not in source

from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def test_bootstrap_plan_core_is_value_only_and_dependency_inward() -> None:
    header = ROOT / "src/bootstrap/HarkenSearchBootstrapPlan.h"
    source = ROOT / "src/bootstrap/HarkenSearchBootstrapPlan.cpp"
    assert header.is_file()
    assert source.is_file()
    text = header.read_text(encoding="utf-8") + source.read_text(encoding="utf-8")
    for forbidden in (
        "windows.h",
        "std::filesystem",
        "CreateProcess",
        "src/update",
        "update/",
        "platform/",
        "main.cpp",
        "WindowsMain",
        "imgui.h",
        "Database",
        "AudioEngine",
    ):
        assert forbidden not in text
    for required in (
        "ValidateSignedInstallPlan",
        "BuildReplacementJournal",
        "RecordUpdatedChildProcess",
        "AcknowledgeHealth",
        "RollbackTransaction",
        "RecoveryBlocked",
    ):
        assert required in text


def test_bootstrap_plan_core_is_registered_as_an_independent_target_and_governed() -> None:
    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    behavior = (ROOT / "cmake/harken_search_behavior_tests.cmake").read_text(encoding="utf-8")
    rules_path = ROOT / "docs/architecture/stage-4-dependency-rules.json"
    rules = rules_path.read_text(encoding="utf-8")
    rules_data = json.loads(rules)
    stage7 = (ROOT / "tools/stage7_api_contract_audit.py").read_text(
        encoding="utf-8"
    )

    assert "add_library(HarkenSearchBootstrapCore STATIC EXCLUDE_FROM_ALL" in cmake
    assert 'ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/$<CONFIG>"' in cmake
    assert "src/bootstrap/HarkenSearchBootstrapPlan.cpp" in cmake
    assert "TestHarkenSearchBootstrapPlan" in behavior
    assert "TestHarkenSearchBootstrapPlanBoundary" in behavior
    bootstrap_paths = {
        "src/bootstrap/HarkenSearchBootstrapPlan.h",
        "src/bootstrap/HarkenSearchBootstrapPlan.cpp",
    }
    assert bootstrap_paths <= set(rules_data["scope"])
    assert bootstrap_paths == set(rules_data["modules"]["bootstrap_core"]["files"])
    assert '"src/bootstrap/HarkenSearchBootstrapPlan.h"' in stage7

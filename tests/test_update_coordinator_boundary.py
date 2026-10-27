from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def test_update_coordinator_is_a_pure_domain_boundary() -> None:
    header = (ROOT / "src/update/UpdateCoordinator.h").read_text(encoding="utf-8")
    source = (ROOT / "src/update/UpdateCoordinator.cpp").read_text(encoding="utf-8")
    state = (ROOT / "src/update/UpdateState.h").read_text(encoding="utf-8")
    joined = header + source + state
    for forbidden in (
        "main.cpp",
        "WindowsMain",
        "#include <windows.h>",
        "#include <wininet.h>",
        "#include <filesystem>",
        "#include <fstream>",
        "#include \"Database.h\"",
        "#include \"imgui.h\"",
        "AudioEngine",
    ):
        assert forbidden not in joined
    assert "UpdateCoordinator" in header
    assert "UpdateSnapshot" in state
    assert "std::variant" not in joined


def test_update_coordinator_is_registered_in_build_and_stage4() -> None:
    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    behavior_cmake = (ROOT / "cmake/harken_search_behavior_tests.cmake").read_text(
        encoding="utf-8"
    )
    rules = (ROOT / "docs/architecture/stage-4-dependency-rules.json").read_text(
        encoding="utf-8"
    )
    assert "src/update/UpdateCoordinator.cpp" in cmake
    assert "TestUpdateCoordinator" in behavior_cmake
    for name in ("UpdateState.h", "UpdateCoordinator.h", "UpdateCoordinator.cpp"):
        assert f'"src/update/{name}"' in rules

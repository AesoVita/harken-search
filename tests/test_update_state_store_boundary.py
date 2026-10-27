from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def test_update_state_store_header_is_owned_value_and_boundary_is_pure() -> None:
    header = (ROOT / "src/update/UpdateStateStore.h").read_text(encoding="utf-8")
    source = (ROOT / "src/update/UpdateStateStore.cpp").read_text(encoding="utf-8")
    for forbidden in (
        "nlohmann",
        "#include <windows.h>",
        "#include <wininet.h>",
        "#include <filesystem>",
        "#include <fstream>",
        "#include \"Database.h\"",
        "settings.ini",
        "main.cpp",
        "WindowsMain",
        "#include \"imgui.h\"",
        "AudioEngine",
    ):
        assert forbidden not in header
    joined = header + source
    for forbidden in (
        "#include <windows.h>",
        "#include <wininet.h>",
        "#include <filesystem>",
        "#include <fstream>",
        "#include \"Database.h\"",
        "#include \"imgui.h\"",
        "AudioEngine",
    ):
        assert forbidden not in joined
    assert "PlanAtomicWrite" in header
    assert "AtomicReplace" in header
    assert "const std::string& storageRoot" not in header
    assert "nlohmann" in source


def test_update_state_store_is_registered_in_product_tests_and_governance() -> None:
    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    behavior = (ROOT / "cmake/harken_search_behavior_tests.cmake").read_text(encoding="utf-8")
    rules = (ROOT / "docs/architecture/stage-4-dependency-rules.json").read_text(
        encoding="utf-8"
    )
    assert "src/update/UpdateStateStore.cpp" in cmake
    assert "TestUpdateStateStore" in behavior
    assert '"src/update/UpdateStateStore.h"' in rules
    assert '"src/update/UpdateStateStore.cpp"' in rules

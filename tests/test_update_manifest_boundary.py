from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def test_update_manifest_is_pure_and_one_way() -> None:
    header = (ROOT / "src/update/UpdateManifest.h").read_text(encoding="utf-8")
    source = (ROOT / "src/update/UpdateManifest.cpp").read_text(encoding="utf-8")
    joined = header + source
    for forbidden in (
        "main.cpp",
        "WindowsMain",
        "windows.h",
        "wininet.h",
        "AppState",
        "Database",
        "ImGui",
        "AuroWidgets",
    ):
        assert forbidden not in joined
    assert "VerifyAndParseManifest" in header
    assert "crypto_sign_open" in source


def test_update_manifest_is_in_product_and_test_builds() -> None:
    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    behavior_cmake = (ROOT / "cmake/harken_search_behavior_tests.cmake").read_text(
        encoding="utf-8"
    )
    rules_path = ROOT / "docs/architecture/stage-4-dependency-rules.json"
    rules = json.loads(rules_path.read_text(encoding="utf-8"))
    assert "src/update/UpdateManifest.cpp" in cmake
    assert "TestUpdateManifest" in behavior_cmake
    manifest_paths = {
        "src/update/UpdateManifest.cpp",
        "src/update/UpdateManifest.h",
    }
    assert manifest_paths <= set(rules["scope"])
    assert manifest_paths <= set(rules["modules"]["domain_policy"]["files"])

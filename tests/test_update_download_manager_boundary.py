from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def test_update_download_manager_is_transport_and_platform_free() -> None:
    header = (ROOT / "src/update/UpdateDownloadManager.h").read_text(encoding="utf-8")
    source = (ROOT / "src/update/UpdateDownloadManager.cpp").read_text(encoding="utf-8")
    joined = header + source
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
        "HttpClient",
    ):
        assert forbidden not in joined
    assert "UpdateDownloadManager" in header
    assert "RenamePartToTarget" in header
    assert "DeletePart" in header
    assert "DeleteMeta" in header
    assert "updateRoot" not in joined
    assert "strip_prefix" not in joined
    assert "stripPrefix" not in joined


def test_update_download_manager_is_registered_in_product_tests_and_stage4() -> None:
    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    behavior = (ROOT / "cmake/harken_search_behavior_tests.cmake").read_text(encoding="utf-8")
    rules = (ROOT / "docs/architecture/stage-4-dependency-rules.json").read_text(
        encoding="utf-8"
    )
    assert "src/update/UpdateDownloadManager.cpp" in cmake
    assert "TestUpdateDownloadManager" in behavior
    assert '"src/update/UpdateDownloadManager.h"' in rules
    assert '"src/update/UpdateDownloadManager.cpp"' in rules

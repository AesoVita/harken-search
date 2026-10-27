from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def test_runtime_is_application_service_without_platform_or_hidden_scheduler() -> None:
    header = ROOT / "src/update/UpdateRuntime.h"
    source = ROOT / "src/update/UpdateRuntime.cpp"
    assert header.is_file()
    assert source.is_file()
    text = header.read_text(encoding="utf-8") + source.read_text(encoding="utf-8")
    for forbidden in (
        "windows.h", "wininet.h", "Sleep(", "std::filesystem", "HINTERNET",
        "main.cpp", "WindowsMain", "imgui.h", "Database", "AudioEngine",
        "updateRoot", "storageRoot", "std::thread",
    ):
        assert forbidden not in text
    for required in (
        "UpdateCoordinator", "UpdateDownloadManager", "UpdateHttpTransport",
        "HttpTransportPort", "UpdatePersistencePort", "UpdateStateStore",
        "UpdateRetryIntent", "RunScheduledRetry", "DeactivateTransfer",
    ):
        assert required in text


def test_runtime_is_registered_in_product_tests_and_governance() -> None:
    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    behavior = (ROOT / "cmake/harken_search_behavior_tests.cmake").read_text(encoding="utf-8")
    rules = (ROOT / "docs/architecture/stage-4-dependency-rules.json").read_text(
        encoding="utf-8"
    )
    stage7 = (ROOT / "tools/stage7_api_contract_audit.py").read_text(encoding="utf-8")
    assert "src/update/UpdateRuntime.cpp" in cmake
    assert "TestUpdateRuntime" in behavior
    assert "TestUpdateRuntimeBoundary" in behavior
    assert '"src/update/UpdateRuntime.h"' in rules
    assert '"src/update/UpdateRuntime.cpp"' in rules
    assert '"src/update/UpdateRuntime.h"' in stage7

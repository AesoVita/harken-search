from __future__ import annotations

import json
import pytest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def test_update_check_service_is_threadless_pure_and_uses_dedicated_fetch_port() -> None:
    header = (ROOT / "src/update/UpdateCheckService.h").read_text(encoding="utf-8")
    source = (ROOT / "src/update/UpdateCheckService.cpp").read_text(encoding="utf-8")
    joined = header + source

    # Contract: no thread, sleep, clock global, Windows/platform include,
    # filesystem/root/path, main.cpp/WindowsMain, UI, DB, audio, or webui.
    for forbidden in (
        "#include <thread>",
        "std::thread",
        "std::jthread",
        "Sleep(",
        "sleep_for(",
        "#include <chrono>",
        "steady_clock",
        "system_clock",
        "windows.h",
        "Windows.h",
        "platform/",
        "std::filesystem",
        "updateRoot",
        "storageRoot",
        "absolutePath",
        "relativePath",
        "main.cpp",
        "WindowsMain",
        "UIRenderer",
        "ImGui",
        "AppState",
        "Database",
        "AudioEngine",
        "webui",
    ):
        assert forbidden not in joined

    assert "UpdateCheckTransportPort" in header
    transport = (ROOT / "src/update/UpdateCheckTransport.h").read_text(
        encoding="utf-8"
    )
    assert "std::vector<std::uint8_t> body" in transport
    assert "HttpTransportPort" not in joined
    assert "nlohmann" not in joined
    assert "const UpdateCheckServiceConfig config_" in header
    assert "CheckNow(\n        const UpdateCheckCancellation& cancellation)" in header


def test_manifest_fetch_precedes_signature_and_committed_verifier() -> None:
    source = (ROOT / "src/update/UpdateCheckService.cpp").read_text(
        encoding="utf-8"
    )
    manifest_fetch = source.index("{config_.manifestUrl")
    signature_fetch = source.index("{config_.signatureUrl")
    verification = source.index("VerifyAndParseManifest(")
    assert manifest_fetch < signature_fetch < verification
    assert "IsLowerHexSignature" in source
    assert "CandidateFrom(verified.manifest)" in source


def test_service_is_registered_in_product_tests() -> None:
    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    behavior = (ROOT / "cmake/harken_search_behavior_tests.cmake").read_text(
        encoding="utf-8"
    )
    assert "src/update/UpdateCheckService.cpp" in cmake
    assert "TestUpdateCheckService" in behavior
    assert "TestUpdateCheckServiceBoundary" in behavior


@pytest.mark.skipif(not (ROOT / "docs/architecture/stage-4-dependency-rules.json").is_file(), reason="Historical architecture ledger is retained in the private repository")
def test_internal_service_stage4_governance() -> None:
    rules = json.loads(
        (ROOT / "docs/architecture/stage-4-dependency-rules.json").read_text(
            encoding="utf-8"
        )
    )

    paths = {
        "src/update/UpdateCheckService.h",
        "src/update/UpdateCheckService.cpp",
        "src/update/UpdateCheckTransport.h",
    }
    assert paths <= set(rules["scope"])
    assert {
        "src/update/UpdateCheckService.h",
        "src/update/UpdateCheckService.cpp",
    } <= set(rules["modules"]["update_application_service"]["files"])
    assert "src/update/UpdateCheckTransport.h" in rules["modules"]["update_platform_service"]["files"]

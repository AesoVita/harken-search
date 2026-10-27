from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
COMPOSITION = ROOT / "src/update/HarkenSearchUpdateComposition.cpp"
BOOTSTRAP_MAIN_HEADER = ROOT / "src/bootstrap/HarkenSearchBootstrapMain.h"
BOOTSTRAP_RUNTIME_HEADER = ROOT / "src/bootstrap/HarkenSearchBootstrapRuntime.h"
HEALTH_HEADER = ROOT / "src/platform/update_bootstrap_health_port.h"
HEALTH_SOURCE = ROOT / "src/platform/update_bootstrap_health_port.cpp"
CAPACITY_HEADER = ROOT / "src/platform/update_staging_capacity_port.h"
CAPACITY_SOURCE = ROOT / "src/platform/update_staging_capacity_port.cpp"


def function_body(source: str, signature: str) -> str:
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening:index + 1]
    raise AssertionError(f"unclosed function body: {signature}")


def test_health_and_capacity_ports_are_neutral_and_composition_free_of_native_details() -> None:
    composition = COMPOSITION.read_text(encoding="utf-8")
    for token in (
        "GetStdHandle",
        "GetFileType",
        "PeekNamedPipe",
        "ReadFile",
        "GetDiskFreeSpaceExW",
        "class Capacity final",
        "bool IsPipe(",
        "bool ReadAvailable(",
    ):
        assert token not in composition, f"composition still owns migrated token: {token}"

    for path in (HEALTH_HEADER, HEALTH_SOURCE, CAPACITY_HEADER, CAPACITY_SOURCE):
        assert path.is_file(), f"missing platform seam: {path.relative_to(ROOT)}"

    for path in (HEALTH_HEADER, CAPACITY_HEADER):
        text = path.read_text(encoding="utf-8")
        for forbidden in ("windows.h", "Windows.h", "HANDLE", "DWORD", "ULARGE_INTEGER", "std::filesystem"):
            assert forbidden not in text


def test_health_port_separates_one_shot_challenge_capture_from_ready_ack() -> None:
    header = HEALTH_HEADER.read_text(encoding="utf-8")
    source = HEALTH_SOURCE.read_text(encoding="utf-8")
    assert "kChallengeWaitMs = 120'000U" in source
    assert "Accepted" in header
    assert "Unsupported" in header
    assert "InvalidChannel" in header
    assert "Timeout" in header
    assert "IoFailure" in header
    assert "CreateUpdateBootstrapHealthPort" in header
    assert "virtual Result CaptureChallenge() = 0;" in header
    assert "virtual Result AcknowledgeReady() = 0;" in header

    capture = function_body(source, "Result CaptureChallenge() override")
    acknowledgement = function_body(source, "Result AcknowledgeReady() override")
    for token in (
        'value.size() != 3U',
        '"build_id"',
        '"version"',
        '"token"',
        "ReadAvailable",
        "captureAttempted_",
        "captured_",
        "buildId_",
        "version_",
        "token_",
    ):
        assert token in capture
    assert "WriteFile" not in capture

    for token in (
        '"process_id"',
        '"ui_ready", true',
        "WriteFile",
        "!captured_ || acknowledged_",
        "acknowledged_ = true",
        "buildId_",
        "version_",
        "token_",
    ):
        assert token in acknowledgement
    assert '"appReady"' not in acknowledgement
    assert '"identity"' not in acknowledgement
    assert "ReadAvailable" not in acknowledgement
    for token in (
        "bool captureAttempted_ = false",
        "bool captured_ = false",
        "bool acknowledged_ = false",
        'Status::Unsupported',
    ):
        assert token in source


def test_bootstrap_health_wait_defaults_allow_slow_first_start() -> None:
    assert "healthWaitMs = 120'000U" in BOOTSTRAP_MAIN_HEADER.read_text(
        encoding="utf-8"
    )
    assert "healthWaitMs = 120'000U" in BOOTSTRAP_RUNTIME_HEADER.read_text(
        encoding="utf-8"
    )


def test_capacity_factory_has_owned_result_and_platform_selection() -> None:
    header = CAPACITY_HEADER.read_text(encoding="utf-8")
    source = CAPACITY_SOURCE.read_text(encoding="utf-8")
    assert "UpdateStagingCapacityPort" in header
    assert "CreateUpdateStagingCapacityPort" in header
    assert "GetDiskFreeSpaceExW" in source
    assert "CreateStatus::Unsupported" in source
    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    behavior = (ROOT / "cmake/harken_search_behavior_tests.cmake").read_text(encoding="utf-8")
    for path in (
        "src/platform/update_bootstrap_health_port.cpp",
        "src/platform/update_staging_capacity_port.cpp",
    ):
        assert path in cmake
    assert behavior.count("NAME TestUpdateBootstrapHealthCapacityBoundary") == 1

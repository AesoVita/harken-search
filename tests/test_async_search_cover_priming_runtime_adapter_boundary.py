import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/AsyncSearchCoverPrimingRuntimeAdapter.h"
SOURCE = ROOT / "src/AsyncSearchCoverPrimingRuntimeAdapter.cpp"
MAIN = ROOT / "src/main.cpp"
FACTORY = ROOT / "src/AsyncSearchResultEffectFactory.cpp"


def test_cover_priming_runtime_owns_state_machine_and_frame_budget() -> None:
    assert HEADER.is_file(), f"missing header: {HEADER}"
    assert SOURCE.is_file(), f"missing source: {SOURCE}"
    header = HEADER.read_text(encoding="utf-8")
    source = SOURCE.read_text(encoding="utf-8")
    assert "StartDeferredCoverPriming();" in header
    assert "PumpDeferredCoverPriming(" in header
    for owned in (
        "g_coverPrimeJob",
        "state.audioEngine.isPlaying()",
        "state.scanReadyToWorkPending",
        "state.warmupFramesRemaining",
        "state.dbSwitchSettleFramesRemaining",
        "CoverModule::PrimeFileCoverKeysFromDb",
        "DevTools_LogCoverPrimingMs",
    ):
        assert owned in source
    for forbidden in ("main.cpp", "WindowsMain", "platform/"):
        assert forbidden not in header
        assert forbidden not in source


def test_root_only_starts_and_pumps_owned_runtime() -> None:
    main = MAIN.read_text(encoding="utf-8")
    factory = FACTORY.read_text(encoding="utf-8")
    assert '#include "AsyncSearchCoverPrimingRuntimeAdapter.h"' in main
    assert "StartDeferredCoverPriming" in factory
    assert "aura::search::PumpDeferredCoverPriming(state, 15);" in main
    for moved in (
        "} g_coverPrimeJob;",
        "static void StartDeferredCoverPriming()",
        "static void PumpDeferredCoverPriming(",
        "static void PrimeCoverKeyCachesFromRecords(",
    ):
        assert moved not in main


def test_cover_priming_runtime_is_built_and_governed() -> None:
    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    behavior = (ROOT / "cmake/harken_search_behavior_tests.cmake").read_text(encoding="utf-8")
    rules = json.loads(
        (ROOT / "docs/architecture/stage-4-dependency-rules.json").read_text(
            encoding="utf-8"
        )
    )
    stage7 = (ROOT / "tools/stage7_api_contract_audit.py").read_text(
        encoding="utf-8"
    )
    assert "src/AsyncSearchCoverPrimingRuntimeAdapter.cpp" in cmake
    assert "TestAsyncSearchCoverPrimingRuntimeAdapterBoundary" in behavior
    for path in (
        "src/AsyncSearchCoverPrimingRuntimeAdapter.cpp",
        "src/AsyncSearchCoverPrimingRuntimeAdapter.h",
    ):
        assert path in rules["scope"]
        assert path in rules["modules"]["composition"]["files"]
    assert (
        '"src/AsyncSearchCoverPrimingRuntimeAdapter.h": '
        '"tests/test_async_search_cover_priming_runtime_adapter_boundary.py"'
    ) in stage7

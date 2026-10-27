import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/AsyncSearchRefreshResultCompositionAdapter.h"
SOURCE = ROOT / "src/AsyncSearchRefreshResultCompositionAdapter.cpp"
MAIN = ROOT / "src/AsyncSearchResultApplicationAdapter.cpp"


def test_refresh_composition_adapter_owns_ordered_refresh_pipeline() -> None:
    assert HEADER.is_file(), f"missing header: {HEADER}"
    assert SOURCE.is_file(), f"missing source: {SOURCE}"

    header = HEADER.read_text(encoding="utf-8")
    source = SOURCE.read_text(encoding="utf-8")
    assert "class AsyncSearchRefreshResultCompositionAdapter final" in header
    assert "void apply()" in header
    for include in (
        '#include "AsyncSearchBrowsePreviewRefreshStateAdapter.h"',
        '#include "AsyncSearchRefreshResultsStateAdapter.h"',
        '#include "AsyncSearchRefreshCountRepairAdapter.h"',
        '#include "AsyncSearchRefreshFacetsAdapter.h"',
        '#include "AsyncSearchEmptyFallbackStateAdapter.h"',
        '#include "AsyncSearchCoverPreloadStateAdapter.h"',
        '#include "AsyncSearchWaveformPreloadStateAdapter.h"',
        '#include "AsyncSearchRefreshCompletionStateAdapter.h"',
        '#include "RandomShufflePlaybackStateAdapter.h"',
        '#include "AsyncSearchRefreshDiagnosticsStateAdapter.h"',
        '#include "AsyncSearchRefreshLoadingStateAdapter.h"',
        '#include "AsyncSearchRefreshMemoryStateAdapter.h"',
    ):
        assert include in source
    for owned in (
        "browsePreviewAdapter.capture()",
        "refreshResultsAdapter.apply()",
        "countRepairAdapter.apply()",
        "facetsAdapter.apply()",
        "emptyFallbackAdapter.apply()",
        "browsePreviewAdapter.restore",
        "coverPreloadAdapter.apply()",
        "waveformPreloadAdapter.apply()",
        "refreshCompletionAdapter.apply()",
        "randomShuffleAdapter.apply()",
        "refreshDiagnosticsAdapter.apply()",
        "refreshLoadingAdapter.apply()",
        "memoryAdapter.apply()",
    ):
        assert owned in source
    for forbidden in ("main.cpp", "WindowsMain", "platform/"):
        assert forbidden not in header
        assert forbidden not in source


def test_apply_async_search_result_delegates_refresh_branch_once() -> None:
    main = MAIN.read_text(encoding="utf-8")
    assert '#include "AsyncSearchRefreshResultCompositionAdapter.h"' in main
    body = main
    assert body.count("refreshCompositionAdapter.apply();") == 1
    refresh_block = body
    assert "AsyncSearchRefreshResultCompositionAdapter" in refresh_block
    for moved in (
        "AsyncSearchRefreshResultsStateAdapter",
        "AsyncSearchRefreshCountRepairAdapter",
        "AsyncSearchRefreshFacetsAdapter",
        "AsyncSearchEmptyFallbackStateAdapter",
        "AsyncSearchCoverPreloadStateAdapter",
        "AsyncSearchWaveformPreloadStateAdapter",
        "AsyncSearchRefreshCompletionStateAdapter",
        "AsyncSearchRandomShufflePlaybackStateAdapter",
        "AsyncSearchRefreshDiagnosticsStateAdapter",
        "AsyncSearchRefreshLoadingStateAdapter",
        "AsyncSearchRefreshMemoryStateAdapter",
    ):
        assert moved not in refresh_block


def test_refresh_composition_adapter_is_governed() -> None:
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
    assert "src/AsyncSearchRefreshResultCompositionAdapter.cpp" in cmake
    assert "TestAsyncSearchRefreshResultCompositionAdapterBoundary" in behavior
    for path in (
        "src/AsyncSearchRefreshResultCompositionAdapter.cpp",
        "src/AsyncSearchRefreshResultCompositionAdapter.h",
    ):
        assert path in rules["scope"]
        assert path in rules["modules"]["composition"]["files"]
    assert (
        '"src/AsyncSearchRefreshResultCompositionAdapter.h": '
        '"tests/test_async_search_refresh_result_composition_adapter_boundary.py"'
    ) in stage7

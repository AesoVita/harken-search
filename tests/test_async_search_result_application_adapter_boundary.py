import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/AsyncSearchResultApplicationAdapter.h"
SOURCE = ROOT / "src/AsyncSearchResultApplicationAdapter.cpp"
MAIN = ROOT / "src/main.cpp"


def test_application_adapter_owns_complete_result_application_flow() -> None:
    assert HEADER.is_file(), f"missing header: {HEADER}"
    assert SOURCE.is_file(), f"missing source: {SOURCE}"

    header = HEADER.read_text(encoding="utf-8")
    source = SOURCE.read_text(encoding="utf-8")
    assert "class AsyncSearchResultApplicationAdapter final" in header
    assert "void apply()" in header
    for include in (
        '#include "AsyncSearchResultDispatchWorkflow.h"',
        '#include "AsyncSearchLoadAllResultStateAdapter.h"',
        '#include "AsyncSearchResultLifecycleStateAdapter.h"',
        '#include "AsyncSearchPlaylistContextStateAdapter.h"',
        '#include "AsyncSearchFacetResultStateAdapter.h"',
        '#include "AsyncSearchPlaylistResultPreparationStateAdapter.h"',
        '#include "AsyncSearchRefreshResultCompositionAdapter.h"',
        '#include "AsyncSearchAppendResultStateAdapter.h"',
        '#include "AsyncSearchQueryTimingRegistry.h"',
    ):
        assert include in source
    for owned in (
        "finishMilliseconds(result_.requestId)",
        "ClassifyAsyncSearchResultDispatch",
        "loadAllResultAdapter.acceptsResult()",
        "lifecycleAdapter.apply()",
        "playlistContextAdapter.apply()",
        "facetResultAdapter.apply()",
        "playlistPreparationAdapter.apply()",
        "refreshCompositionAdapter.apply()",
        "appendResultAdapter.apply()",
        "loadAllResultAdapter.observeResult",
        "state_.syncToTab()",
    ):
        assert owned in source
    for forbidden in ("main.cpp", "WindowsMain", "platform/"):
        assert forbidden not in header
        assert forbidden not in source


def test_product_root_only_assembles_result_application_adapter() -> None:
    main = MAIN.read_text(encoding="utf-8")
    assert '#include "AsyncSearchResultApplicationWorkflow.h"' in main
    body = main.split(
        "BuildAsyncSearchResultApplicationEffects() {",
        1,
    )[1].split("\n}", 1)[0]
    assert "AsyncSearchResultApplicationAdapter" not in body
    assert "applicationAdapter.apply();" not in body
    for moved in (
        "finishMilliseconds(",
        "ClassifyAsyncSearchResultDispatch",
        "AsyncSearchLoadAllResultStateAdapter",
        "AsyncSearchResultLifecycleStateAdapter",
        "AsyncSearchPlaylistContextStateAdapter",
        "AsyncSearchFacetResultStateAdapter",
        "AsyncSearchPlaylistResultPreparationStateAdapter",
        "AsyncSearchRefreshResultCompositionAdapter",
        "AsyncSearchAppendResultStateAdapter",
        "state.syncToTab()",
    ):
        assert moved not in body


def test_application_adapter_is_built_and_governed() -> None:
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
    assert "src/AsyncSearchResultApplicationAdapter.cpp" in cmake
    assert "TestAsyncSearchResultApplicationAdapterBoundary" in behavior
    for path in (
        "src/AsyncSearchResultApplicationAdapter.cpp",
        "src/AsyncSearchResultApplicationAdapter.h",
    ):
        assert path in rules["scope"]
        assert path in rules["modules"]["composition"]["files"]
    assert (
        '"src/AsyncSearchResultApplicationAdapter.h": '
        '"tests/test_async_search_result_application_adapter_boundary.py"'
    ) in stage7

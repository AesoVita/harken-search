from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
FACTORY = (ROOT / "src" / "AsyncSearchResultEffectFactory.cpp").read_text(encoding="utf-8")
HEADER_PATH = ROOT / "src" / "CoverKeyFilterStateAdapter.h"
SOURCE_PATH = ROOT / "src" / "CoverKeyFilterStateAdapter.cpp"
CMAKE = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
BEHAVIOR = (ROOT / "cmake" / "harken_search_behavior_tests.cmake").read_text(
    encoding="utf-8"
)
RULES = (
    ROOT / "docs" / "architecture" / "stage-4-dependency-rules.json"
).read_text(encoding="utf-8")
STAGE7 = (ROOT / "tools" / "stage7_api_contract_audit.py").read_text(
    encoding="utf-8"
)


def test_cover_key_filter_state_adapter_owns_app_state_projection():
    assert HEADER_PATH.exists()
    assert SOURCE_PATH.exists()

    header = HEADER_PATH.read_text(encoding="utf-8")
    source = SOURCE_PATH.read_text(encoding="utf-8")

    assert "void ApplyCoverKeyFilter(AppState& state);" in header
    assert '#include "AppState.h"' in source
    assert '#include "CoverKeyFilterWorkflow.h"' in source
    assert '#include "CoverModule.h"' in source
    assert "CoverModule::FileHasCoverKey(filePath, key)" in source
    assert "state.coverFilterActive && !state.coverFilterPath.empty()" in source
    assert "state.coverFilterMatchSig != 0" in source
    assert "state.coverFilterMatchPaths.find(record.filePath)" in source
    assert "HandleCoverKeyFilter(input, effects)" in source
    assert "filtered.push_back(std::move(state.searchResults[index]));" in source
    assert "state.loadedCount = static_cast<int>(state.searchResults.size());" in source
    assert "state.bumpSearchResultsGen();" in source

    for forbidden in ("main.cpp", "WindowsMain", "platform/"):
        assert forbidden not in header
        assert forbidden not in source


def test_root_injects_cover_key_filter_without_owning_projection():
    assert '#include "CoverKeyFilterStateAdapter.h"' in MAIN
    assert "static void ApplyCoverKeyFilterInPlace(" not in MAIN
    assert "CoverKeyFilterInput" not in MAIN
    assert "CoverKeyFilterRecord" not in MAIN
    assert "HandleCoverKeyFilter(" not in MAIN
    assert FACTORY.count("ApplyCoverKeyFilter,") == 2


def test_cover_key_filter_state_adapter_is_built_tested_and_governed():
    assert CMAKE.count("src/CoverKeyFilterStateAdapter.cpp") == 1
    assert BEHAVIOR.count(
        "tests/test_cover_key_filter_state_adapter_boundary.py"
    ) == 1
    assert RULES.count("src/CoverKeyFilterStateAdapter.cpp") >= 2
    assert RULES.count("src/CoverKeyFilterStateAdapter.h") >= 2
    assert (
        '"src/CoverKeyFilterStateAdapter.h": '
        '"tests/test_cover_key_filter_state_adapter_boundary.py"'
        in STAGE7
    )

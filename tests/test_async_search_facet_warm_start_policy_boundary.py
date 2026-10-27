from pathlib import Path
import json

ROOT = Path(__file__).resolve().parents[1]

def test_facet_warm_start_policy_owns_root_predicate():
    header = (ROOT / "src/AsyncSearchFacetWarmStartPolicy.h").read_text()
    source = (ROOT / "src/AsyncSearchFacetWarmStartPolicy.cpp").read_text()
    main = (ROOT / "src/main.cpp").read_text()
    assert "IsFilteredFacetWarmStartCountStateImpossible" in header
    assert "cover_replacement::IsFilteredFacetWarmStartCountStateImpossible" in source
    assert "IsFilteredFacetWarmStartCountStateImpossibleForCoverWall" not in main
    assert '#include "AsyncSearchFacetWarmStartPolicy.h"' in main

def test_facet_warm_start_policy_has_one_way_dependencies():
    for path in [ROOT / "src/AsyncSearchFacetWarmStartPolicy.h",
                 ROOT / "src/AsyncSearchFacetWarmStartPolicy.cpp"]:
        text = path.read_text()
        assert "main.cpp" not in text
        assert "WindowsMain" not in text
        assert "platform/" not in text

def test_facet_warm_start_policy_is_governed():
    cmake = (ROOT / "CMakeLists.txt").read_text()
    rules = json.loads(
        (ROOT / "docs/architecture/stage-4-dependency-rules.json")
        .read_text(encoding="utf-8-sig"))
    stage7 = (ROOT / "tools/stage7_api_contract_audit.py").read_text()
    behavior = (ROOT / "cmake/harken_search_behavior_tests.cmake").read_text()
    assert "AsyncSearchFacetWarmStartPolicy.cpp" in cmake
    assert "src/AsyncSearchFacetWarmStartPolicy.cpp" in rules["scope"]
    assert "AsyncSearchFacetWarmStartPolicy.h" in stage7
    assert "TestAsyncSearchFacetWarmStartPolicyBoundary" in behavior

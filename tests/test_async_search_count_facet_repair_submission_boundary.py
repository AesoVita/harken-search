from pathlib import Path
import json

ROOT = Path(__file__).resolve().parents[1]

def test_count_facet_submission_owns_request_and_lifecycle():
    header = (ROOT / "src/AsyncSearchCountFacetRepairSubmission.h").read_text()
    source = (ROOT / "src/AsyncSearchCountFacetRepairSubmission.cpp").read_text()
    main = (ROOT / "src/main.cpp").read_text()
    assert "SubmitCountAndFacetRepair" in header
    assert "AsyncSearchKind::CountAndFacetsOnly" in source
    assert "pendingFacetRequestId" in source
    assert "queryTiming.begin" in source
    assert "static uint64_t SubmitAsyncCountAndFacetRepair" not in main
    assert '#include "AsyncSearchCountFacetRepairSubmission.h"' in main

def test_count_facet_submission_has_no_reverse_dependency():
    for path in [ROOT / "src/AsyncSearchCountFacetRepairSubmission.h",
                 ROOT / "src/AsyncSearchCountFacetRepairSubmission.cpp"]:
        text = path.read_text()
        assert "main.cpp" not in text
        assert "WindowsMain" not in text
        assert "platform/" not in text

def test_count_facet_submission_is_governed():
    cmake = (ROOT / "CMakeLists.txt").read_text()
    rules = json.loads(
        (ROOT / "docs/architecture/stage-4-dependency-rules.json")
        .read_text(encoding="utf-8-sig"))
    stage7 = (ROOT / "tools/stage7_api_contract_audit.py").read_text()
    behavior = (ROOT / "cmake/harken_search_behavior_tests.cmake").read_text()
    assert "AsyncSearchCountFacetRepairSubmission.cpp" in cmake
    assert "src/AsyncSearchCountFacetRepairSubmission.cpp" in rules["scope"]
    assert "AsyncSearchCountFacetRepairSubmission.h" in stage7
    assert "TestAsyncSearchCountFacetRepairSubmissionBoundary" in behavior

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/AsyncSearchQueryTimingRegistry.h"
SOURCE = ROOT / "src/AsyncSearchQueryTimingRegistry.cpp"
MAIN = ROOT / "src/main.cpp"
PUMP = ROOT / "src/AsyncSearchResultPumpWorkflow.cpp"
APPLICATION = ROOT / "src/AsyncSearchResultApplicationAdapter.cpp"


def test_query_timing_registry_owns_search_start_times() -> None:
    assert HEADER.exists()
    assert SOURCE.exists()
    main = MAIN.read_text(encoding="utf-8")
    count_facet_submission = (
        ROOT / "src/AsyncSearchCountFacetRepairSubmission.cpp"
    ).read_text(encoding="utf-8")
    pump = PUMP.read_text(encoding="utf-8")
    application = APPLICATION.read_text(encoding="utf-8")
    assert "AsyncSearchQueryTimingRegistry g_searchQueryTiming" in main
    assert main.count("g_searchQueryTiming.begin(") == 1
    assert count_facet_submission.count("queryTiming.begin(") == 1
    assert application.count("queryTiming_.finishMilliseconds(") == 1
    assert pump.count("queryTiming.clear()") == 1
    assert "g_searchStartTimes" not in main


def test_apply_result_has_no_clock_or_registry_details() -> None:
    application = APPLICATION.read_text(encoding="utf-8")
    assert "steady_clock::now()" not in application
    assert ".find(result.requestId)" not in application
    assert ".erase(" not in application

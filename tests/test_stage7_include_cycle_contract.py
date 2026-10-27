from tools import stage7_api_contract_audit as audit


def test_cycle_detector_reports_stable_cycle() -> None:
    graph = {
        "src/a.h": {"src/b.h"},
        "src/b.h": {"src/a.h"},
        "src/c.h": set(),
    }

    assert audit.find_include_cycles(graph) == [["src/a.h", "src/b.h", "src/a.h"]]

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/CoverKeyFilterWorkflow.h"
SOURCE = ROOT / "src/CoverKeyFilterWorkflow.cpp"
MAIN = ROOT / "src/main.cpp"
FACTORY = ROOT / "src/AsyncSearchResultEffectFactory.cpp"
STATE_ADAPTER = ROOT / "src/CoverKeyFilterStateAdapter.cpp"


def test_cover_key_filter_workflow_has_pure_predicate_and_root_delegation() -> None:
    assert HEADER.exists()
    assert SOURCE.exists()
    source = SOURCE.read_text(encoding="utf-8")
    main = MAIN.read_text(encoding="utf-8")
    factory = FACTORY.read_text(encoding="utf-8")
    state_adapter = STATE_ADAPTER.read_text(encoding="utf-8")
    assert "class CoverKeyFilterEffects" in HEADER.read_text(encoding="utf-8")
    assert "HandleCoverKeyFilter(" in source
    assert "HandleCoverKeyFilter(" in state_adapter
    assert "HandleCoverKeyFilter(" not in main
    assert "static void ApplyCoverKeyFilterInPlace(" not in main
    assert factory.count("ApplyCoverKeyFilter,") == 2

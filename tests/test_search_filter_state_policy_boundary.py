from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "src/main.cpp"
HEADER = ROOT / "src/SearchFilterStatePolicy.h"
SOURCE = ROOT / "src/SearchFilterStatePolicy.cpp"


def test_advanced_filter_policy_has_independent_owner():
    assert HEADER.exists()
    assert SOURCE.exists()
    text = SOURCE.read_text(encoding="utf-8")
    assert "HasAdvancedDbFilters" in text
    assert "searchBuffer" in text
    assert "formatFilterIndex" in text


def test_main_does_not_define_advanced_filter_policy():
    text = MAIN.read_text(encoding="utf-8")
    assert "static bool HasAdvancedDbFilters(const AppState& state) {" not in text

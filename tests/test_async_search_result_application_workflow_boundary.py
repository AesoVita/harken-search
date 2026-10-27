from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "src" / "main.cpp"
HEADER = ROOT / "src" / "AsyncSearchResultApplicationWorkflow.h"
SOURCE = ROOT / "src" / "AsyncSearchResultApplicationWorkflow.cpp"


def test_application_workflow_owns_result_application_entrypoint():
    assert HEADER.exists()
    assert SOURCE.exists()
    text = SOURCE.read_text(encoding="utf-8")
    assert "namespace aura::search" in text
    assert "Apply(" in text
    assert "AsyncSearchResultApplicationAdapter" in text


def test_main_does_not_define_result_application_function():
    text = MAIN.read_text(encoding="utf-8")
    assert "static void ApplyAsyncSearchResult(AppState& state" not in text

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "src/main.cpp"
HEADER = ROOT / "src/SearchKeywordCompiler.h"
SOURCE = ROOT / "src/SearchKeywordCompiler.cpp"


def test_search_keyword_compiler_has_independent_owner():
    assert HEADER.exists()
    assert SOURCE.exists()
    text = SOURCE.read_text(encoding="utf-8")
    assert "CompileSlashSearchInputToKeyword" in text
    assert "compiled.ok" in text


def test_main_does_not_define_keyword_compiler():
    text = MAIN.read_text(encoding="utf-8")
    assert "static std::string CompileSearchBufferKeywordForFilter(" not in text

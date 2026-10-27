from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "src/main.cpp"
HEADER = ROOT / "src/DatabasePathNormalizationPolicy.h"
SOURCE = ROOT / "src/DatabasePathNormalizationPolicy.cpp"


def test_database_path_normalization_has_independent_owner():
    assert HEADER.exists()
    assert SOURCE.exists()
    text = SOURCE.read_text(encoding="utf-8")
    assert "Utf8ToPath" in text
    assert "PathToUtf8" in text
    assert "catch (...)" in text


def test_main_does_not_define_database_path_normalization():
    text = MAIN.read_text(encoding="utf-8")
    assert "static std::string NormalizeFolderPathForDb(" not in text

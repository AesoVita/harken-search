from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "src/main.cpp"
HEADER = ROOT / "src/SearchFilterFileTypeAdapter.h"
SOURCE = ROOT / "src/SearchFilterFileTypeAdapter.cpp"


def test_file_type_adapter_owns_filter_projection():
    assert HEADER.exists()
    assert SOURCE.exists()
    text = SOURCE.read_text(encoding="utf-8")
    assert "FileTypeVisibility" in text
    assert "BuildPolicy" in text
    assert "excludeExtensions.insert" in text
    assert "includeOnlyKnownExts" in text


def test_main_does_not_define_file_type_filter_projection():
    text = MAIN.read_text(encoding="utf-8")
    assert "static void ApplyFileTypePolicyToFilter(" not in text

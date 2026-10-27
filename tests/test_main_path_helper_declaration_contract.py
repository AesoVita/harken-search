from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def test_utf8_path_helper_is_declared_before_settings_file_helpers_use_it():
    source = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")

    declaration = "static fs::path Utf8ToPathMain(const std::string& str);"
    assert declaration in source
    assert source.index(declaration) < source.index("static std::unordered_map<std::string, std::string> LoadKeyValueFile")

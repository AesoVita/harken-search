# -*- coding: utf-8 -*-
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
SRC = ROOT / "src"

ALLOWED = {
    "src/platform/harken_legacy_data_migrator.cpp",
    "src/platform/harken_legacy_data_migrator.h",
}

SRC_SUFFIXES = {".cpp", ".h", ".hpp", ".inl", ".cc", ".c"}

OLD_DIR_TOKENS = (
    "AuroAudio",
    "ArisAudio",
    "Aris" + chr(92) + chr(92) + "updates",
    "Aris/updates",
    "ProgramData" + chr(92) + chr(92) + "Aris" + chr(92) + chr(92) + "updates",
    chr(92) + chr(92) + "Auro" + chr(92) + chr(92),
    "/Auro/",
)


def _old_directory_tokens(code):
    # Identifier names (for example a legacy association constant) are not
    # filesystem addresses. Inspect only the actual C++ string literals.
    literals = re.findall(r'"(?:\\.|[^"\\])*"', code)
    return [token for token in OLD_DIR_TOKENS if any(token in value for value in literals)]

def _iter_src_files():
    for path in SRC.rglob("*"):
        if not path.is_file():
            continue
        if path.suffix.lower() not in SRC_SUFFIXES:
            continue
        rel = path.relative_to(ROOT).as_posix()
        if "macos" in path.name.lower() or path.suffix.lower() == ".mm":
            continue
        if rel.startswith("src/webui/"):
            continue
        yield path, rel

class HarkenLegacyDataDirScanTests(unittest.TestCase):
    def test_old_data_directory_literals_live_only_in_migrator_legacy_list(self):
        hits = []
        for path, rel in _iter_src_files():
            if rel in ALLOWED:
                continue
            text = path.read_text(encoding="utf-8")
            for line_no, raw in enumerate(text.splitlines(), 1):
                code = raw.split("//", 1)[0]
                if "kAuroAudioExts" in code:
                    continue
                for token in _old_directory_tokens(code):
                    hits.append("%s:%d:%s:%s" % (rel, line_no, token, raw.strip()[:160]))
        self.assertEqual(hits, [], "old data-directory literals must stay in the migrator legacy list:" + chr(10) + chr(10).join(hits[:80]))

    def test_association_identifier_is_not_a_directory_but_literal_is(self):
        self.assertEqual(_old_directory_tokens('const char* kLegacyAuroAudioFileAssociation = "Auro.AudioFile";'), [])
        self.assertEqual(_old_directory_tokens('const char* folder = "AuroAudio";'), ["AuroAudio"])

    def test_migrator_legacy_list_names_every_old_directory(self):
        path = ROOT / "src" / "platform" / "harken_legacy_data_migrator.cpp"
        self.assertTrue(path.is_file(), "migrator source is missing")
        text = path.read_text(encoding="utf-8")
        required = ("AuroAudio", "ArisAudio", "Auro" + chr(92) + chr(92) + "Database", "Auro" + chr(92) + chr(92) + "auro.db", "Aris" + chr(92) + chr(92) + "updates")
        missing = [token for token in required if token not in text]
        self.assertEqual(missing, [], "migrator legacy list missing: " + ", ".join(missing))
        self.assertIn("HarkenSearch", text)

if __name__ == "__main__":
    unittest.main()

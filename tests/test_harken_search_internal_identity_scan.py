import pathlib
import re
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
SCAN_ROOTS = (ROOT / "src", ROOT / "cmake", ROOT / "reaper_extension")
OLD_PATH = re.compile(r"(?i)(?:^|[/\\])(aris|auro|aura)(?:[-_A-Za-z0-9]|[/\\]|$)")


class HarkenSearchInternalIdentityScanTests(unittest.TestCase):
    def test_product_source_paths_have_no_legacy_prefix(self):
        hits = []
        for base in SCAN_ROOTS:
            for path in base.rglob("*"):
                if OLD_PATH.search(path.relative_to(ROOT).as_posix()):
                    hits.append(path.relative_to(ROOT).as_posix())
        self.assertEqual(hits, [], "legacy product source paths remain:\n" + "\n".join(hits))

    def test_product_symbols_have_no_legacy_prefix(self):
        hits = []
        for base in SCAN_ROOTS:
            for path in base.rglob("*"):
                if not path.is_file() or path.suffix.lower() not in {".cpp", ".h", ".hpp", ".c", ".cmake"}:
                    continue
                if path.name == "harken_legacy_data_migrator.cpp":
                    continue
                text = path.read_text(encoding="utf-8")
                for line_no, line in enumerate(text.splitlines(), 1):
                    code = line.split("//", 1)[0]
                    code = re.sub(r'"(?:\\.|[^"\\])*"', '""', code)
                    if path.suffix.lower() == ".cmake":
                        code = code.split("#", 1)[0]
                        # The existing generated-localization build target is
                        # a shared entry point, not a product symbol or copy.
                        # Keep this one exact target usable by all test TUs.
                        code = re.sub(r"\bArisLocalizationGenerated\b", "", code)
                    if re.search(r"(?<!Legacy)\b(?:Auro|Aris|Aura)(?:::|[A-Z_][A-Za-z0-9_]*)", code):
                        hits.append(f"{path.relative_to(ROOT).as_posix()}:{line_no}:{line.strip()}")
        self.assertEqual(hits, [], "legacy product symbols remain:\n" + "\n".join(hits[:120]))


if __name__ == "__main__":
    unittest.main()

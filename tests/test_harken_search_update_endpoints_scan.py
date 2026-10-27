import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]

CLIENT_GLOBS = (
    "src/update/**/*",
    "src/bootstrap/**/*",
    "src/platform/update*.cpp",
    "src/platform/update*.h",
    "src/WindowsMain.cpp",
    "CMakeLists.txt",
    "tools/prepare_local_update_demo.py",
)

CLIENT_SUFFIXES = {".cpp", ".h", ".hpp", ".inl", ".cc", ".c", ".txt", ".py"}

# New-client retrieval/UA/demo constants that shipped Aris EXEs still use.
FORBIDDEN_CLIENT = (
    ("Aris Update/1", re.compile(r"Aris Update/")),
    ("Aris Update Check/1", re.compile(r"Aris Update Check/")),
    ("local Aris update zip", re.compile(r"Aris-\{version\}-win-x64-update\.zip")),
    ("concatenated Aris update zip", re.compile(r'"Aris-"\s*\+\s*binding\.version\(\)\s*\+\s*"-win-x64-update\.zip"')),
    ("aris-demo namespace", re.compile(r"/aris-demo/")),
    ("aris-demo CLI flag", re.compile(r"--aris-demo-cli")),
)

REQUIRED_CLIENT = (
    "Harken Search Update/1",
    "Harken Search Update Check/1",
    "HarkenSearch-{version}-win-x64-update.zip",
    '"HarkenSearch-" + binding.version() + "-win-x64-update.zip"',
    "http://127.0.0.1:4328/harken-search-demo/",
    "--harken-search-demo-cli",
)

# Publisher dual-write of the old remote zip name. Old clients follow the
# signed manifest URL / COS object key; do not delete these until old builds
# leave support.
COMPAT_SUPPLY = (
    ("tools/release_update.py", "Aris-{version}-win-x64-update.zip", "旧客户端兼容供给"),
    ("tools/build_update_manifest.py", "Aris-{version}-win-x64-update.zip", "旧客户端兼容供给"),
    ("tests/release_contract/fixtures/expected_update_release_contract.json", "Aris-{version}-win-x64-update.zip", "旧客户端兼容供给"),
    ("tests/release_contract/test_update_release_pipeline_contract.py", "Aris-{version}-win-x64-update.zip", "旧客户端兼容供给"),
    ("tests/release_contract/test_update_release_pipeline_behavior_contract.py", "Aris-1.2.3-win-x64-update.zip", "旧客户端兼容供给"),
)

REQUIRED_COMPAT = "Aris-{version}-win-x64-update.zip"
REQUIRED_NEW_PUBLISHED_ALIAS = "HarkenSearch-{version}-win-x64-update.zip"


def _iter_client_files():
    seen = set()
    for pattern in CLIENT_GLOBS:
        for path in ROOT.glob(pattern):
            if not path.is_file():
                continue
            if path.suffix.lower() not in CLIENT_SUFFIXES and path.name != "CMakeLists.txt":
                continue
            rel = path.relative_to(ROOT).as_posix()
            if rel in seen:
                continue
            seen.add(rel)
            yield path, rel


class HarkenSearchUpdateEndpointScanTests(unittest.TestCase):
    def test_new_client_drops_old_retrieval_constants_and_uses_harken_forms(self):
        hits = []
        blob = {}
        for path, rel in _iter_client_files():
            text = path.read_text(encoding="utf-8")
            blob[rel] = text
            for label, rx in FORBIDDEN_CLIENT:
                for match in rx.finditer(text):
                    line_no = text.count("\n", 0, match.start()) + 1
                    hits.append("%s:%d:%s:%s" % (rel, line_no, label, match.group(0)))
        self.assertEqual(
            hits,
            [],
            "new client still has old retrieval constants:\n" + "\n".join(hits[:80]),
        )
        missing = []
        joined = "\n".join(blob.values())
        for token in REQUIRED_CLIENT:
            if token not in joined:
                missing.append(token)
        self.assertEqual(missing, [], "required Harken Search update identity missing: " + ", ".join(missing))

    def test_publisher_keeps_annotated_old_zip_supply_and_adds_harken_alias(self):
        for rel, token, note in COMPAT_SUPPLY:
            path = ROOT / rel
            self.assertTrue(path.is_file(), rel)
            text = path.read_text(encoding="utf-8")
            self.assertIn(token, text, "%s missing %s (%s)" % (rel, token, note))
        publisher = (ROOT / "tools/release_update.py").read_text(encoding="utf-8")
        self.assertIn(REQUIRED_COMPAT, publisher)
        self.assertIn(REQUIRED_NEW_PUBLISHED_ALIAS, publisher)
        self.assertIn("旧客户端兼容供给", publisher)


if __name__ == "__main__":
    unittest.main()

import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
SCAN_ROOTS = ("src", "tests", "tools", "installer", "docs")
SKIP_SUFFIXES = {
    ".dll",
    ".exe",
    ".gif",
    ".ico",
    ".jpeg",
    ".jpg",
    ".lib",
    ".obj",
    ".pdb",
    ".png",
    ".wav",
    ".webp",
    ".zip",
}

TOKEN_RE = re.compile("ATTA" + r"[0-9A-Za-z]{20,}")
KEY_RE = re.compile("669aeddd" + "ef2efa19")
HOST_RE = re.compile("trello" + r"\.com", re.IGNORECASE)


def iter_scanned_files():
    for name in SCAN_ROOTS:
        root = ROOT / name
        if not root.exists():
            continue
        for path in root.rglob("*"):
            if not path.is_file():
                continue
            if "__pycache__" in path.parts:
                continue
            if path.suffix.lower() in SKIP_SUFFIXES or path.suffix.lower() == ".pyc":
                continue
            yield path


class TrelloDeadcodeScanTests(unittest.TestCase):
    def test_patterns_match_known_shapes_without_storing_literals(self):
        token = "ATTA" + ("xyz0" * 8)
        short_token = "ATTA" + "xyz0"
        key = "669aeddd" + "ef2efa19"
        host = "api." + "trello" + ".com"
        self.assertIsNotNone(TOKEN_RE.search(token))
        self.assertIsNone(TOKEN_RE.search(short_token))
        self.assertIsNotNone(KEY_RE.search(key))
        self.assertIsNotNone(HOST_RE.search(host))
        self.assertIsNone(HOST_RE.search("example.com"))

    def test_src_tests_tools_installer_docs_have_no_trello_secrets_or_host(self):
        hits = []
        for path in iter_scanned_files():
            try:
                text = path.read_text(encoding="utf-8")
            except UnicodeDecodeError:
                text = path.read_bytes().decode("utf-8", errors="replace")
            rel = path.relative_to(ROOT).as_posix()
            for label, rx in (("token", TOKEN_RE), ("key", KEY_RE), ("host", HOST_RE)):
                for match in rx.finditer(text):
                    hits.append(f"{rel}:{label}:offset={match.start()}:len={len(match.group(0))}")
        self.assertEqual(hits, [], "forbidden leftovers remain:\n" + "\n".join(hits))


if __name__ == "__main__":
    unittest.main()

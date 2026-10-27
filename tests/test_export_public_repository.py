"""Public source seeds must contain only the selected committed file bytes."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
EXPORTER = ROOT / "tools/export_public_repository.py"


class ExportPublicRepositoryTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="public-seed-test-")
        self.addCleanup(self.temporary.cleanup)
        self.base = Path(self.temporary.name)
        self.repo = self.base / "source"
        self.repo.mkdir()
        self.git("init", "-q")
        self.git("config", "user.name", "Fixture")
        self.git("config", "user.email", "fixture@example.invalid")
        self.git("config", "core.autocrlf", "false")
        for name, payload in {
            "README.md": b"public readme\n",
            "src/main.cpp": b"approved bytes\n",
            "docs/context/private.md": b"private fixture must stay local\n",
        }.items():
            path = self.repo / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(payload)
        self.git("add", "README.md", "src/main.cpp", "docs/context/private.md")
        self.git("commit", "-qm", "Fixture source")
        self.commit = self.git("rev-parse", "HEAD").strip()
        self.output = self.base / "public"
        self.report = self.base / "receipt.json"

    def git(self, *args):
        return subprocess.check_output(["git", "-C", str(self.repo), *args], text=True)

    def export(self, revision=None):
        return subprocess.run(
            [sys.executable, str(EXPORTER), "--root", str(self.repo),
             "--revision", revision or self.commit, "--output", str(self.output),
             "--report", str(self.report)],
            capture_output=True, text=True, encoding="utf-8",
        )

    def test_exports_pinned_commit_without_private_history_or_uncommitted_edits(self):
        (self.repo / "src/main.cpp").write_bytes(b"unreviewed work in progress\n")
        (self.repo / "src/untracked.cpp").write_bytes(b"untracked secret fixture\n")
        result = self.export()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.output / "src/main.cpp").read_bytes(), b"approved bytes\n")
        self.assertEqual(sorted(p.relative_to(self.output).as_posix() for p in self.output.rglob("*") if p.is_file()),
                         ["README.md", "src/main.cpp"])
        self.assertFalse((self.output / ".git").exists())
        receipt = json.loads(self.report.read_text(encoding="utf-8"))
        self.assertEqual(receipt["source_commit"], self.commit)
        self.assertEqual(receipt["excluded_count"], 1)
        item = next(row for row in receipt["files"] if row["path"] == "src/main.cpp")
        self.assertEqual(item["sha256"], hashlib.sha256(b"approved bytes\n").hexdigest())

    def test_refuses_to_overwrite_an_existing_destination(self):
        self.output.mkdir()
        (self.output / "keep.txt").write_text("keep", encoding="utf-8")
        result = self.export()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("empty", result.stderr.lower())
        self.assertEqual((self.output / "keep.txt").read_text(), "keep")

    def test_rejects_an_invalid_revision_without_creating_a_public_tree(self):
        result = self.export("missing-revision")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("revision", result.stderr.lower())
        self.assertFalse(self.output.exists())
        self.assertFalse(self.report.exists())

    def test_rejects_public_symbolic_links_before_writing_files(self):
        blob = subprocess.check_output(["git", "-C", str(self.repo), "hash-object", "-w", "--stdin"], input=b"../docs/context/private.md").decode().strip()
        self.git("update-index", "--add", "--cacheinfo", f"120000,{blob},src/link")
        self.git("commit", "-qm", "Unsafe link fixture")
        result = self.export(self.git("rev-parse", "HEAD").strip())
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("unsupported", result.stderr.lower())
        self.assertFalse(self.output.exists())


if __name__ == "__main__":
    unittest.main()

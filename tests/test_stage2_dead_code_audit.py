from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / "tools" / "stage2_dead_code_audit.py"


class AuditFixture:
    def __init__(self, root: Path) -> None:
        self.root = root

    def write(self, relative_path: str, content: str | bytes) -> None:
        path = self.root / relative_path
        path.parent.mkdir(parents=True, exist_ok=True)
        if isinstance(content, bytes):
            path.write_bytes(content)
        else:
            path.write_text(content, encoding="utf-8")

    def track_all(self) -> None:
        subprocess.run(
            ["git", "init", "-q"],
            cwd=self.root,
            check=True,
            capture_output=True,
            text=True,
        )
        subprocess.run(
            ["git", "config", "core.autocrlf", "false"],
            cwd=self.root,
            check=True,
            capture_output=True,
            text=True,
        )
        subprocess.run(
            ["git", "add", "-A"],
            cwd=self.root,
            check=True,
            capture_output=True,
            text=True,
        )

    def remove(self, relative_path: str) -> None:
        subprocess.run(
            ["git", "rm", "-q", "-f", "--", relative_path],
            cwd=self.root,
            check=True,
            capture_output=True,
            text=True,
        )


class Stage2DeadCodeAuditTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temp_dir = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp_dir.cleanup)
        self.root = Path(self.temp_dir.name)
        self.fixture = AuditFixture(self.root)
        self.register = self.root / "register.json"
        self.report = self.root / "report.md"

    def run_audit(self, *extra_args: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [
                sys.executable,
                str(TOOL),
                "--root",
                str(self.root),
                "--register",
                str(self.register),
                "--output",
                str(self.report),
                *extra_args,
            ],
            cwd=ROOT,
            capture_output=True,
            text=True,
            check=False,
        )

    def load_candidates(self) -> dict[str, dict]:
        document = json.loads(self.register.read_text(encoding="utf-8"))
        return {item["path"]: item for item in document["candidates"]}

    def make_reference_fixture(self) -> None:
        self.fixture.write(
            "CMakeLists.txt",
            "add_executable(App src/live.cpp)\n"
            "target_sources(App PRIVATE resources/live.dat)\n",
        )
        self.fixture.write(
            "src/live.cpp",
            '#include "live.h"\nconst char* asset = "resources/live.dat";\n',
        )
        self.fixture.write("src/live.h", "#pragma once\n")
        self.fixture.write("src/orphan.cpp", "static int orphan() { return 1; }\n")
        self.fixture.write("resources/live.dat", b"live")
        self.fixture.write("resources/orphan.dat", b"orphan")
        self.fixture.write("scripts/manual_probe.py", "print('manual')\n")
        self.fixture.write(
            "docs/plans/audit-note.md",
            "The audit note mentions src/orphan.cpp but does not make it reachable.\n",
        )
        self.fixture.write(
            "thirdparty/vendor.cpp",
            "#if 0\nint retiredVendor() { return 0; }\n#endif\nint vendor() { return 1; }\n",
        )
        self.fixture.write("LICENSES/vendor.txt", "vendor license\n")
        self.fixture.track_all()

    def test_reference_graph_only_reports_unreferenced_first_party_files(self) -> None:
        self.make_reference_fixture()

        result = self.run_audit()

        self.assertEqual(0, result.returncode, result.stderr)
        candidates = self.load_candidates()
        self.assertIn("src/orphan.cpp", candidates)
        self.assertIn("resources/orphan.dat", candidates)
        self.assertIn("scripts/manual_probe.py", candidates)
        self.assertNotIn("src/live.cpp", candidates)
        self.assertNotIn("src/live.h", candidates)
        self.assertNotIn("resources/live.dat", candidates)
        self.assertNotIn("thirdparty/vendor.cpp", candidates)
        self.assertNotIn("LICENSES/vendor.txt", candidates)

    def test_string_reference_counts_as_an_incoming_edge(self) -> None:
        self.fixture.write("CMakeLists.txt", "add_executable(App src/main.cpp)\n")
        self.fixture.write(
            "src/main.cpp",
            'const char* lateResource = "resources/late-load.dat";\n',
        )
        self.fixture.write("resources/late-load.dat", b"late")
        self.fixture.track_all()

        result = self.run_audit()

        self.assertEqual(0, result.returncode, result.stderr)
        self.assertNotIn("resources/late-load.dat", self.load_candidates())

    def test_existing_keep_disposition_survives_rescan(self) -> None:
        self.make_reference_fixture()
        first = self.run_audit()
        self.assertEqual(0, first.returncode, first.stderr)
        document = json.loads(self.register.read_text(encoding="utf-8"))
        candidate = next(
            item for item in document["candidates"] if item["path"] == "src/orphan.cpp"
        )
        candidate.update(
            {
                "disposition": "KEEP",
                "dynamic_lookup_checked": True,
                "compatibility_checked": True,
                "reviewer": "stage2-review",
                "reason": "manual entry point",
            }
        )
        self.register.write_text(
            json.dumps(document, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )

        second = self.run_audit()

        self.assertEqual(0, second.returncode, second.stderr)
        self.assertEqual("KEEP", self.load_candidates()["src/orphan.cpp"]["disposition"])

    def test_review_user_decision_survives_a_new_incoming_reference(self) -> None:
        self.make_reference_fixture()
        first = self.run_audit()
        self.assertEqual(0, first.returncode, first.stderr)
        document = json.loads(self.register.read_text(encoding="utf-8"))
        candidate = next(
            item for item in document["candidates"] if item["path"] == "src/orphan.cpp"
        )
        candidate.update(
            {
                "disposition": "REVIEW_USER",
                "dynamic_lookup_checked": True,
                "compatibility_checked": True,
                "reviewer": "stage2-review",
                "reason": "product contract needs a decision",
            }
        )
        self.register.write_text(
            json.dumps(document, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )
        self.fixture.write(
            "tests/preservation_gate.py",
            'PRESERVED_PATH = "src/orphan.cpp"\n',
        )
        self.fixture.track_all()

        second = self.run_audit()

        self.assertEqual(0, second.returncode, second.stderr)
        candidates = self.load_candidates()
        self.assertIn("src/orphan.cpp", candidates, "待用户裁决项不得因新增保护引用而丢失")
        self.assertEqual("REVIEW_USER", candidates["src/orphan.cpp"]["disposition"])

    def test_rescan_records_an_unstaged_file_deletion_as_absent(self) -> None:
        self.make_reference_fixture()
        first = self.run_audit()
        self.assertEqual(0, first.returncode, first.stderr)
        document = json.loads(self.register.read_text(encoding="utf-8"))
        candidate = next(
            item for item in document["candidates"] if item["path"] == "src/orphan.cpp"
        )
        candidate.update(
            {
                "disposition": "DELETE",
                "evidence": [
                    {"type": "E1", "source": "compiler", "detail": "unused local"},
                    {"type": "E2", "source": "reference graph", "detail": "no entry"},
                ],
                "dynamic_lookup_checked": True,
                "compatibility_checked": True,
                "reviewer": "stage2-review",
                "reason": "no build or runtime entry",
            }
        )
        self.register.write_text(
            json.dumps(document, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )
        (self.root / "src/orphan.cpp").unlink()

        second = self.run_audit()

        self.assertEqual(0, second.returncode, second.stderr)
        self.assertFalse(self.load_candidates()["src/orphan.cpp"]["present"])

    def test_outputs_are_byte_stable_for_the_same_tree(self) -> None:
        self.make_reference_fixture()
        first = self.run_audit()
        self.assertEqual(0, first.returncode, first.stderr)
        first_register = self.register.read_bytes()
        first_report = self.report.read_bytes()

        second = self.run_audit()

        self.assertEqual(0, second.returncode, second.stderr)
        self.assertEqual(first_register, self.register.read_bytes())
        self.assertEqual(first_report, self.report.read_bytes())

    def test_review_gate_rejects_pending_and_under_evidenced_deletions(self) -> None:
        self.make_reference_fixture()
        initial = self.run_audit("--check-reviewed")
        self.assertNotEqual(0, initial.returncode)
        self.assertIn("PENDING", initial.stderr)

        document = json.loads(self.register.read_text(encoding="utf-8"))
        for item in document["candidates"]:
            item.update(
                {
                    "disposition": "KEEP",
                    "dynamic_lookup_checked": True,
                    "compatibility_checked": True,
                    "reviewer": "stage2-review",
                    "reason": "retained by review",
                }
            )
        target = next(
            item for item in document["candidates"] if item["path"] == "src/orphan.cpp"
        )
        target["disposition"] = "DELETE"
        target["evidence"] = [
            {"type": "E2", "source": "reference_graph", "detail": "zero incoming edges"}
        ]
        self.register.write_text(
            json.dumps(document, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )

        result = self.run_audit("--check-reviewed")

        self.assertNotEqual(0, result.returncode)
        self.assertIn("two evidence types", result.stderr)

    def test_closed_gate_requires_reviewed_delete_to_be_absent(self) -> None:
        self.make_reference_fixture()
        first = self.run_audit()
        self.assertEqual(0, first.returncode, first.stderr)
        document = json.loads(self.register.read_text(encoding="utf-8"))
        for item in document["candidates"]:
            item.update(
                {
                    "disposition": "KEEP",
                    "dynamic_lookup_checked": True,
                    "compatibility_checked": True,
                    "reviewer": "stage2-review",
                    "reason": "retained by review",
                }
            )
        target = next(
            item for item in document["candidates"] if item["path"] == "src/orphan.cpp"
        )
        target.update(
            {
                "disposition": "DELETE",
                "evidence": [
                    {
                        "type": "E1",
                        "source": "compiler",
                        "detail": "unreferenced local function",
                    },
                    {
                        "type": "E2",
                        "source": "reference_graph",
                        "detail": "zero incoming edges",
                    },
                ],
                "reason": "no build or runtime entry",
            }
        )
        self.register.write_text(
            json.dumps(document, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )

        still_present = self.run_audit("--check-closed")
        self.assertNotEqual(0, still_present.returncode)
        self.assertIn("still present", still_present.stderr)

        self.fixture.remove("src/orphan.cpp")
        removed = self.run_audit("--check-closed")

        self.assertEqual(0, removed.returncode, removed.stderr)
        candidate = self.load_candidates()["src/orphan.cpp"]
        self.assertFalse(candidate["present"])
        self.assertEqual("DELETE", candidate["disposition"])

    def test_zero_coverage_file_is_recorded_as_supporting_evidence(self) -> None:
        self.fixture.write(
            "CMakeLists.txt",
            "add_executable(App src/covered.cpp src/zero.cpp)\n",
        )
        self.fixture.write("src/covered.cpp", "int covered() { return 1; }\n")
        self.fixture.write("src/zero.cpp", "int zero() { return 0; }\n")
        self.fixture.track_all()
        coverage = self.root / "coverage.xml"
        coverage.write_text(
            """<?xml version="1.0"?>
<coverage><packages><package><classes>
  <class filename="src/covered.cpp"><lines><line number="1" hits="1"/></lines></class>
  <class filename="src/zero.cpp"><lines><line number="1" hits="0"/></lines></class>
</classes></package></packages></coverage>
""",
            encoding="utf-8",
        )

        result = self.run_audit("--coverage", str(coverage))

        self.assertEqual(0, result.returncode, result.stderr)
        candidates = self.load_candidates()
        self.assertNotIn("src/covered.cpp", candidates)
        self.assertIn("src/zero.cpp", candidates)
        self.assertIn(
            "E3",
            {item["type"] for item in candidates["src/zero.cpp"]["evidence"]},
        )

    def test_compiler_and_linker_findings_create_symbol_candidates(self) -> None:
        self.fixture.write("CMakeLists.txt", "add_executable(App src/unused.cpp)\n")
        self.fixture.write(
            "src/unused.cpp",
            "static int orphan() { return 1; }\nint main() { return 0; }\n",
        )
        self.fixture.track_all()
        compiler_log = self.root / "compiler.log"
        compiler_log.write_text(
            f"{self.root / 'src' / 'unused.cpp'}(1,12): warning C4505: "
            f"'orphan': unreferenced local function has been removed [{self.root / 'build' / 'App.vcxproj'}]\n",
            encoding="utf-16",
        )
        linker_log = self.root / "linker.log"
        linker_log.write_text(
            "      已丢弃 ?orphan@@YAHXZ(从 unused.obj 中)\n",
            encoding="utf-16",
        )

        result = self.run_audit(
            "--compiler-log",
            str(compiler_log),
            "--linker-log",
            str(linker_log),
        )

        self.assertEqual(0, result.returncode, result.stderr)
        document = json.loads(self.register.read_text(encoding="utf-8"))
        symbol_candidates = [
            item
            for item in document["candidates"]
            if item["path"] == "src/unused.cpp" and item["kind"] == "symbol"
        ]
        self.assertEqual(1, len(symbol_candidates))
        evidence_sources = {
            evidence["source"] for evidence in symbol_candidates[0]["evidence"]
        }
        self.assertIn("compiler C4505", evidence_sources)
        self.assertIn("linker OPT:REF", evidence_sources)
        evidence_details = "\n".join(
            evidence["detail"] for evidence in symbol_candidates[0]["evidence"]
        )
        self.assertNotIn(str(self.root), evidence_details)

    def test_disabled_preprocessor_block_is_a_branch_candidate(self) -> None:
        self.fixture.write("CMakeLists.txt", "add_executable(App src/main.cpp)\n")
        self.fixture.write(
            "src/main.cpp",
            "#if 0\nstatic int retired() { return 1; }\n#endif\nint main() { return 0; }\n",
        )
        self.fixture.track_all()

        result = self.run_audit()

        self.assertEqual(0, result.returncode, result.stderr)
        document = json.loads(self.register.read_text(encoding="utf-8"))
        branches = [
            item
            for item in document["candidates"]
            if item["path"] == "src/main.cpp" and item["kind"] == "branch"
        ]
        self.assertEqual(1, len(branches))
        self.assertIn("E5", {item["type"] for item in branches[0]["evidence"]})

    def test_curly_quoted_compiler_symbols_stay_distinct(self) -> None:
        self.fixture.write("CMakeLists.txt", "add_executable(App src/cover.cpp)\n")
        self.fixture.write(
            "src/cover.cpp",
            "static int first() { return 1; }\nstatic int second() { return 2; }\n",
        )
        self.fixture.track_all()
        compiler_log = self.root / "compiler.log"
        compiler_log.write_text(
            f"{self.root / 'src' / 'cover.cpp'}(1,1): warning C4505: "
            "“Scope::`anonymous-namespace'::First”: unreferenced function\n"
            f"{self.root / 'src' / 'cover.cpp'}(2,1): warning C4505: "
            "“Scope::`anonymous-namespace'::Second”: unreferenced function\n",
            encoding="utf-16",
        )

        result = self.run_audit("--compiler-log", str(compiler_log))

        self.assertEqual(0, result.returncode, result.stderr)
        document = json.loads(self.register.read_text(encoding="utf-8"))
        symbols = {
            item["symbol"]
            for item in document["candidates"]
            if item["path"] == "src/cover.cpp" and item["kind"] == "symbol"
        }
        self.assertEqual(
            {
                "Scope::`anonymous-namespace'::First",
                "Scope::`anonymous-namespace'::Second",
            },
            symbols,
        )

    def test_python_import_and_vscode_task_are_incoming_edges(self) -> None:
        self.fixture.write("CMakeLists.txt", "add_executable(App src/main.cpp)\n")
        self.fixture.write("src/main.cpp", "int main() { return 0; }\n")
        self.fixture.write("tools/entry.py", "import helper\n")
        self.fixture.write("tools/helper.py", "VALUE = 1\n")
        self.fixture.write("scripts/package.ps1", "Write-Host 'package'\n")
        self.fixture.write(
            ".vscode/tasks.json",
            '{"command": "powershell -File scripts/package.ps1"}\n',
        )
        self.fixture.track_all()

        result = self.run_audit()

        self.assertEqual(0, result.returncode, result.stderr)
        candidates = self.load_candidates()
        self.assertNotIn("tools/helper.py", candidates)
        self.assertNotIn("scripts/package.ps1", candidates)

    def test_unreferenced_web_source_is_in_the_inventory(self) -> None:
        self.fixture.write("CMakeLists.txt", "add_executable(App src/main.cpp)\n")
        self.fixture.write("src/main.cpp", "int main() { return 0; }\n")
        self.fixture.write("src/webui/orphan.tsx", "export const orphan = 1;\n")
        self.fixture.track_all()

        result = self.run_audit()

        self.assertEqual(0, result.returncode, result.stderr)
        self.assertIn("src/webui/orphan.tsx", self.load_candidates())

    def test_closed_gate_checks_recorded_symbol_against_current_source(self) -> None:
        self.fixture.write("CMakeLists.txt", "add_executable(App src/main.cpp)\n")
        self.fixture.write(
            "src/main.cpp",
            "int main() { int deadValue = 1; return 0; }\n",
        )
        self.fixture.track_all()
        document = {
            "schema_version": 1,
            "root": ".",
            "candidates": [
                {
                    "id": "S2-MANUAL-SYMBOL",
                    "path": "src/main.cpp",
                    "symbol": "deadValue",
                    "kind": "symbol",
                    "present": True,
                    "disposition": "DELETE",
                    "evidence": [
                        {"type": "E1", "source": "compiler", "detail": "unused local"},
                        {"type": "E2", "source": "source review", "detail": "no reads"},
                    ],
                    "contract_refs": [],
                    "dynamic_lookup_checked": True,
                    "compatibility_checked": True,
                    "reviewer": "stage2-review",
                    "reason": "unused local binding",
                    "rollback": "restore the reviewed tranche",
                }
            ],
        }
        self.register.write_text(
            json.dumps(document, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )

        present = self.run_audit("--check-closed")
        self.assertNotEqual(0, present.returncode)
        self.assertIn("still present", present.stderr)

        self.fixture.write("src/main.cpp", "int main() { return 0; }\n")
        absent = self.run_audit("--check-closed")

        self.assertEqual(0, absent.returncode, absent.stderr)


if __name__ == "__main__":
    unittest.main()

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / "tools" / "stage3_human_style_audit.py"


class AuditFixture:
    def __init__(self, root: Path) -> None:
        self.root = root

    def write(self, relative_path: str, content: str | bytes) -> None:
        path = self.root / relative_path
        path.parent.mkdir(parents=True, exist_ok=True)
        if isinstance(content, bytes):
            path.write_bytes(content)
        else:
            path.write_text(content, encoding="utf-8", newline="\n")

    def remove(self, relative_path: str) -> None:
        path = self.root / relative_path
        if path.exists():
            path.unlink()

    def track_all(self) -> None:
        if not (self.root / ".git").exists():
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


class Stage3HumanStyleAuditTest(unittest.TestCase):
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
            encoding="utf-8",
            errors="replace",
            check=False,
        )

    def candidates(self) -> list[dict]:
        return json.loads(self.register.read_text(encoding="utf-8"))["candidates"]

    def by_rule(self, rule: str) -> list[dict]:
        return [item for item in self.candidates() if item["rule"] == rule]

    def test_source_comment_rules_create_review_candidates(self) -> None:
        self.fixture.write(
            "src/state.cpp",
            "// Claude review pass-3: fixed stale state.\n"
            "// 💡 Aura 终极性能优化：彻底解决刷新。\n"
            "// TODO: implement this later.\n"
            "// ====================================================================\n"
            "// FTS rebuild 澶辫触.\n"
            "int state = 1;\n",
        )
        self.fixture.track_all()

        result = self.run_audit()

        self.assertEqual(0, result.returncode, result.stderr)
        rules = {item["rule"] for item in self.candidates()}
        self.assertTrue(
            {
                "assistant_reference",
                "authoring_history",
                "hype_comment",
                "placeholder",
                "decorative_comment",
                "mojibake",
                "terminology",
            }.issubset(rules),
            rules,
        )

    def test_replacement_question_marks_in_comment_are_mojibake(self) -> None:
        self.fixture.write(
            "src/cover.cpp",
            "// ???????????? database artwork lookup ????????????\n",
        )
        self.fixture.track_all()

        result = self.run_audit()

        self.assertEqual(0, result.returncode, result.stderr)
        mojibake = self.by_rule("mojibake")
        self.assertEqual(1, len(mojibake))
        self.assertEqual("src/cover.cpp", mojibake[0]["path"])

    def test_common_gb18030_artifacts_are_mojibake(self) -> None:
        damaged = "初始化封面缓存目录".encode("utf-8").decode("gb18030", errors="ignore")
        self.fixture.write("src/cover.cpp", f"// {damaged}\n")
        self.fixture.track_all()

        result = self.run_audit()

        self.assertEqual(0, result.returncode, result.stderr)
        mojibake = self.by_rule("mojibake")
        self.assertEqual(1, len(mojibake))
        self.assertEqual("src/cover.cpp", mojibake[0]["path"])

    def test_lossless_gb18030_artifacts_and_gitignore_are_scanned(self) -> None:
        damaged_path = "综合".encode("utf-8").decode("gb18030")
        self.fixture.write("docs/proof.md", f"Fixture path: {damaged_path}\\8082\n")
        self.fixture.write(".gitignore", "# Build output \u9225? never commit\n")
        self.fixture.track_all()

        result = self.run_audit()

        self.assertEqual(0, result.returncode, result.stderr)
        paths = {item["path"] for item in self.by_rule("mojibake")}
        self.assertEqual({".gitignore", "docs/proof.md"}, paths)

    def test_lossless_audio_collection_mojibake_is_reported(self) -> None:
        damaged_name = "音效合辑".encode("utf-8").decode("gb18030")
        self.fixture.write(
            "tests/audio_fixture.cpp",
            f'const char* folder = "SoundMorph{damaged_name}";\n',
        )
        self.fixture.track_all()

        result = self.run_audit()

        self.assertEqual(0, result.returncode, result.stderr)
        mojibake = self.by_rule("mojibake")
        self.assertEqual(1, len(mojibake))
        self.assertEqual("tests/audio_fixture.cpp", mojibake[0]["path"])

    def test_code_narration_comment_requires_context_review(self) -> None:
        self.fixture.write(
            "src/audio.cpp",
            "// NOTE: This function simply calls the decoder and returns the result.\n"
            "int DecodeAudio();\n",
        )
        self.fixture.track_all()

        result = self.run_audit()

        self.assertEqual(0, result.returncode, result.stderr)
        narrative = self.by_rule("narrative_comment")
        self.assertEqual(1, len(narrative))
        self.assertEqual("src/audio.cpp", narrative[0]["path"])

    def test_internal_prompt_and_session_prose_is_an_artifact(self) -> None:
        self.fixture.write(
            "docs/process/model-routing.md",
            "Copy this prompt into the next assistant session.\n",
        )
        self.fixture.write(
            "docs/plans/refactor/prompts/launch.md",
            "Start a new conversation with this model prompt.\n",
        )
        self.fixture.write(
            "src/bridge.cpp",
            'const char* help = "Close the Pro Tools save prompt and return to the session window.";\n',
        )
        self.fixture.track_all()

        result = self.run_audit()

        self.assertEqual(0, result.returncode, result.stderr)
        paths = {item["path"] for item in self.by_rule("prompt_artifact")}
        self.assertEqual(
            {
                "docs/plans/refactor/prompts/launch.md",
                "docs/process/model-routing.md",
            },
            paths,
        )

    def test_product_ai_prompt_is_reviewed_instead_of_silently_excluded(self) -> None:
        self.fixture.write(
            "src/AIEngine.cpp",
            'std::string prompt = userPrompt; // User cover-generation prompt.\n',
        )
        self.fixture.write(
            "AI_FEATURE_DISCLOSURE.md",
            "User-entered cover-generation prompts are sent only after explicit action.\n",
        )
        self.fixture.track_all()

        result = self.run_audit()

        self.assertEqual(0, result.returncode, result.stderr)
        prompt_candidates = self.by_rule("prompt_artifact")
        self.assertEqual(
            {"AI_FEATURE_DISCLOSURE.md", "src/AIEngine.cpp"},
            {item["path"] for item in prompt_candidates},
        )
        self.assertTrue(all(item["disposition"] == "PENDING" for item in prompt_candidates))

    def test_legal_vendor_and_generated_paths_are_excluded(self) -> None:
        residue = "// Claude review pass-9: TODO 💡 澶辫触\n"
        self.fixture.write("thirdparty/vendor.cpp", residue)
        self.fixture.write("LICENSES/vendor.txt", residue)
        self.fixture.write("LICENSE", residue)
        self.fixture.write("THIRD_PARTY_NOTICES.md", residue)
        self.fixture.write("ASSET_PROVENANCE.md", residue)
        self.fixture.write("thirdparty/BINARY_PROVENANCE.md", residue)
        self.fixture.write("src/Widget.generated.h", residue)
        self.fixture.write("src/live.cpp", "int live = 1;\n")
        self.fixture.track_all()

        result = self.run_audit()

        self.assertEqual(0, result.returncode, result.stderr)
        self.assertEqual([], self.candidates())

    def test_legacy_identifiers_in_code_and_strings_are_not_prose_hits(self) -> None:
        self.fixture.write(
            "src/compat.cpp",
            'namespace Aura { const char* db = "Auro.db"; }\n'
            'const char* key = "AURO_LEGACY_DATABASE";\n',
        )
        self.fixture.track_all()

        result = self.run_audit()

        self.assertEqual(0, result.returncode, result.stderr)
        self.assertEqual([], self.by_rule("terminology"))

    def test_old_product_name_in_visible_string_is_a_terminology_candidate(self) -> None:
        self.fixture.write(
            "src/gallery.cpp",
            'const char* title = "Auro UI Gallery";\n'
            'const char* database = "Auro.db";\n'
            'const char* protocol = "auro-protools-host-send-v1";\n',
        )
        self.fixture.track_all()

        result = self.run_audit()

        self.assertEqual(0, result.returncode, result.stderr)
        terminology = self.by_rule("terminology")
        self.assertEqual(1, len(terminology))
        self.assertEqual("string", terminology[0]["kind"])
        self.assertEqual("src/gallery.cpp", terminology[0]["path"])

    def test_old_product_name_before_cjk_text_is_a_terminology_candidate(self) -> None:
        self.fixture.write(
            "src/report.cpp",
            'const char* title = u8"Aura测试报告";\n',
        )
        self.fixture.track_all()

        result = self.run_audit()

        self.assertEqual(0, result.returncode, result.stderr)
        terminology = self.by_rule("terminology")
        self.assertEqual(1, len(terminology))
        self.assertEqual("string", terminology[0]["kind"])
        self.assertEqual("src/report.cpp", terminology[0]["path"])

    def test_metaphorical_success_copy_is_reported(self) -> None:
        self.fixture.write(
            "src/cover.cpp",
            'const char* status = u8"封面已应用，它会自动感染目录下的音频。";\n',
        )
        self.fixture.track_all()

        result = self.run_audit()

        self.assertEqual(0, result.returncode, result.stderr)
        candidates = self.by_rule("unprofessional_copy")
        self.assertEqual(1, len(candidates))
        self.assertEqual("string", candidates[0]["kind"])
        self.assertEqual("src/cover.cpp", candidates[0]["path"])

    def test_duplicate_comments_are_grouped_by_path_and_text(self) -> None:
        comment = "// Keep the visible waveform until the replacement decode completes.\n"
        self.fixture.write("src/audio.cpp", comment + "int a = 1;\n" + comment)
        self.fixture.track_all()

        result = self.run_audit()

        self.assertEqual(0, result.returncode, result.stderr)
        duplicates = self.by_rule("duplicate_comment")
        self.assertEqual(1, len(duplicates))
        self.assertEqual("src/audio.cpp", duplicates[0]["path"])
        self.assertEqual([1, 3], duplicates[0]["locations"])

    def test_asterisk_code_continuations_are_not_block_comments(self) -> None:
        self.fixture.write(
            "src/math.cpp",
            "double first = sourceA\n"
            "    * static_cast<double>(sessionSampleRate);\n"
            "double second = sourceB\n"
            "    * static_cast<double>(sessionSampleRate);\n",
        )
        self.fixture.track_all()

        result = self.run_audit()

        self.assertEqual(0, result.returncode, result.stderr)
        self.assertEqual([], self.by_rule("duplicate_comment"))

    def test_inline_parameter_annotations_are_not_duplicate_comments(self) -> None:
        self.fixture.write(
            "src/worker.cpp",
            "if (firstCall(\n"
            "    /*deferFacetInvalidate=*/true) < 0) {}\n"
            "if (secondCall(\n"
            "    /*deferFacetInvalidate=*/true) < 0) {}\n",
        )
        self.fixture.track_all()

        result = self.run_audit()

        self.assertEqual(0, result.returncode, result.stderr)
        self.assertEqual([], self.by_rule("duplicate_comment"))

    def test_delivery_scaffolding_is_reported(self) -> None:
        self.fixture.write(
            "docs/product/cards/card.md",
            "## Agent Staffing\n"
            "**Primary execution agent:** UI\n"
            "**Execution dispatch gate:** TDD required\n"
            "**Agent boundary check:** UI only\n"
            "**Evidence:** chat-only BDD\n"
            "**Review:** advisory only\n"
            "**Staffing decision:** keep current roster\n"
            "**Why this staffing fits the task:** narrow scope\n"
            "**Primary agent:** UI\n"
            "**Additional agents:** none\n"
            "| Agent | Owns what |\n"
            "**User request:** fix search\n"
            "**Evidence:** screenshot in conversation\n"
            "**Durability:** do not rely on chat history\n"
            "**Record:** harness card\n"
            "**Flow:** full-cycle\n"
            "**Known limits:** RED was not retained in this session.\n"
            "**Proof:** 性能证明不能停留在聊天结论。\n"
            "**Scope:** AGENTS-wide refactor\n"
            "**Owner:** dispatch gate\n"
            "**Record:** dispatch brief\n"
            "**Status:** active dispatch card\n"
            "**Review:** before implementation dispatch\n"
            "**Timing:** update the record when dispatched\n"
            "**Next:** dispatch the bounded implementation\n"
            "## Brain Routing\n"
            "## Superpower Route\n",
        )
        self.fixture.track_all()

        result = self.run_audit()

        self.assertEqual(0, result.returncode, result.stderr)
        candidates = self.by_rule("delivery_scaffolding")
        self.assertEqual(27, len(candidates))
        self.assertEqual(
            list(range(1, 28)),
            sorted(candidate["locations"][0] for candidate in candidates),
        )

    def test_hype_rule_does_not_match_command_failure_prose(self) -> None:
        self.fixture.write(
            "docs/report.md",
            "外层 shell 展开导致命令构造失败。\n",
        )
        self.fixture.track_all()

        result = self.run_audit()

        self.assertEqual(0, result.returncode, result.stderr)
        self.assertEqual([], self.by_rule("hype_comment"))

    def test_normal_engineering_history_words_are_not_authoring_residue(self) -> None:
        self.fixture.write(
            "docs/report.md",
            "新增覆盖率测试并修复链接失败。\n",
        )
        self.fixture.track_all()

        result = self.run_audit()

        self.assertEqual(0, result.returncode, result.stderr)
        self.assertEqual([], self.by_rule("authoring_history"))

    def test_bracketed_authoring_labels_are_reported_in_comments(self) -> None:
        self.fixture.write(
            "src/worker.cpp",
            "// 【关键修复3】构建完整请求头。\n",
        )
        self.fixture.write(
            "installer/setup.iss",
            "; 【重要】相对路径基于脚本目录。\n",
        )
        self.fixture.track_all()

        result = self.run_audit()

        self.assertEqual(0, result.returncode, result.stderr)
        candidates = self.by_rule("narrative_comment")
        self.assertEqual(2, len(candidates))
        self.assertEqual(
            {"installer/setup.iss", "src/worker.cpp"},
            {candidate["path"] for candidate in candidates},
        )

    def test_candidate_identity_does_not_depend_on_line_number(self) -> None:
        self.fixture.write("src/ui.cpp", "// TODO: explain the empty state.\nint x = 0;\n")
        self.fixture.track_all()
        first = self.run_audit()
        self.assertEqual(0, first.returncode, first.stderr)
        first_id = self.by_rule("placeholder")[0]["id"]

        self.fixture.write(
            "src/ui.cpp",
            "\n\n// TODO: explain the empty state.\nint x = 0;\n",
        )
        self.fixture.track_all()
        second = self.run_audit()

        self.assertEqual(0, second.returncode, second.stderr)
        candidate = self.by_rule("placeholder")[0]
        self.assertEqual(first_id, candidate["id"])
        self.assertEqual([3], candidate["locations"])

    def test_reviewed_decision_survives_line_movement(self) -> None:
        self.fixture.write("src/ui.cpp", "// TODO: explain the empty state.\nint x = 0;\n")
        self.fixture.track_all()
        first = self.run_audit()
        self.assertEqual(0, first.returncode, first.stderr)

        document = json.loads(self.register.read_text(encoding="utf-8"))
        candidate = next(item for item in document["candidates"] if item["rule"] == "placeholder")
        candidate.update(
            {
                "disposition": "REWRITE",
                "replacement": "The empty state remains visible until a library is selected.",
                "reviewer": "Stage 3 contextual review",
                "reason": "The current marker describes a live UI state but not its invariant.",
            }
        )
        self.register.write_text(json.dumps(document, indent=2), encoding="utf-8")
        self.fixture.write(
            "src/ui.cpp",
            "\n// TODO: explain the empty state.\nint x = 0;\n",
        )
        self.fixture.track_all()

        second = self.run_audit()

        self.assertEqual(0, second.returncode, second.stderr)
        reviewed = self.by_rule("placeholder")[0]
        self.assertEqual("REWRITE", reviewed["disposition"])
        self.assertEqual("Stage 3 contextual review", reviewed["reviewer"])
        self.assertEqual([2], reviewed["locations"])

    def test_check_reviewed_rejects_pending_and_incomplete_decisions(self) -> None:
        self.fixture.write("src/ui.cpp", "// TODO: explain the empty state.\n")
        self.fixture.track_all()

        pending = self.run_audit("--check-reviewed")

        self.assertNotEqual(0, pending.returncode)
        self.assertIn("PENDING", pending.stdout + pending.stderr)

        document = json.loads(self.register.read_text(encoding="utf-8"))
        candidate = next(item for item in document["candidates"] if item["rule"] == "placeholder")
        candidate["disposition"] = "REWRITE"
        candidate["replacement"] = "The empty state remains visible until a library is selected."
        self.register.write_text(json.dumps(document, indent=2), encoding="utf-8")

        incomplete = self.run_audit("--check-reviewed")

        self.assertNotEqual(0, incomplete.returncode)
        self.assertIn("reviewer", incomplete.stdout + incomplete.stderr)

    def test_check_closed_requires_actionable_fingerprint_to_disappear(self) -> None:
        self.fixture.write("src/ui.cpp", "// TODO: explain the empty state.\n")
        self.fixture.track_all()
        first = self.run_audit()
        self.assertEqual(0, first.returncode, first.stderr)

        document = json.loads(self.register.read_text(encoding="utf-8"))
        candidate = next(item for item in document["candidates"] if item["rule"] == "placeholder")
        candidate.update(
            {
                "disposition": "REMOVE",
                "reviewer": "Stage 3 contextual review",
                "reason": "The marker is unsupported and carries no live constraint.",
            }
        )
        self.register.write_text(json.dumps(document, indent=2), encoding="utf-8")

        still_present = self.run_audit("--check-closed")
        self.assertNotEqual(0, still_present.returncode)
        self.assertIn(candidate["id"], still_present.stdout + still_present.stderr)

        self.fixture.write("src/ui.cpp", "int emptyState = 0;\n")
        self.fixture.track_all()
        closed = self.run_audit("--check-reviewed", "--check-closed")

        self.assertEqual(0, closed.returncode, closed.stdout + closed.stderr)
        self.assertNotIn(candidate["id"], {item["id"] for item in self.candidates()})

    def test_markdown_report_does_not_repeat_raw_candidate_text(self) -> None:
        raw = "Claude review pass-7: remove this generated narration."
        self.fixture.write("src/ui.cpp", f"// {raw}\n")
        self.fixture.track_all()

        result = self.run_audit()

        self.assertEqual(0, result.returncode, result.stderr)
        report = self.report.read_text(encoding="utf-8")
        self.assertNotIn(raw, report)
        self.assertIn("assistant_reference", report)
        self.assertIn("authoring_history", report)

    def test_untracked_files_do_not_enter_the_register(self) -> None:
        self.fixture.write("src/live.cpp", "int live = 1;\n")
        self.fixture.track_all()
        self.fixture.write("src/scratch.cpp", "// TODO: untracked scratch file.\n")

        result = self.run_audit()

        self.assertEqual(0, result.returncode, result.stderr)
        self.assertNotIn("src/scratch.cpp", {item["path"] for item in self.candidates()})

    def test_generic_private_name_is_a_candidate_but_bridge_helper_is_not(self) -> None:
        self.fixture.write(
            "src/ui.cpp",
            "static float SidebarLerpHelper(float a, float b, float t) { return a + (b-a)*t; }\n"
            "float hover = SidebarLerpHelper(0.0f, 1.0f, 0.5f);\n"
            "BridgeProcess ResolveBridgeHelper();\n"
            "0x2300, 0x23FF,  // Miscellaneous Technical\n",
        )
        self.fixture.track_all()

        result = self.run_audit()

        self.assertEqual(0, result.returncode, result.stderr)
        generic = self.by_rule("generic_name")
        self.assertEqual(1, len(generic))
        self.assertEqual("src/ui.cpp", generic[0]["path"])


if __name__ == "__main__":
    unittest.main()

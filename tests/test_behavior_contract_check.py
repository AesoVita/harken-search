import sys
import tempfile
import unittest
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import behavior_contract_check  # noqa: E402
import behavior_regression_suite  # noqa: E402


def make_contract(
    *,
    status="Active",
    bdd="- Given 已有前置条件\n- When 用户执行操作\n- Then 系统返回结果",
    proof_row="| TEST-META | unit | `TestMetadataFields` | pass | active |",
    change_gate="- [x] checked",
):
    return (
        "# Test Contract\n\n"
        "**ID:** BDD-TEST\n"
        "**Module:** Test\n"
        f"**Status:** {status}\n"
        "**Priority:** P1\n"
        "**Owner:** Engineering\n"
        "**Created:** 2026-07-11\n"
        "**Last Updated:** 2026-07-11\n\n"
        "## Product Behavior\n\n"
        "**User promise:** test\n**Primary workflow:** test\n**Non-goals:** test\n\n"
        "## 中文语义\n\n"
        "**用户承诺：** 测试\n**核心流程：** 测试\n**非目标：** 测试\n\n"
        "## BDD Scenarios\n\n"
        f"{bdd}\n\n"
        "## Interface Binding\n\n"
        "| Interface / Entry | Kind | Source | Expected behavior |\n"
        "| --- | --- | --- | --- |\n\n"
        "## Graphify Binding\n\nUNKNOWN_BLOCKING entries must be reviewed.\n\n"
        "## TDD / QA Proof\n\n"
        "| Proof ID | Kind | Artifact / command / event | Pass signal | Status |\n"
        "| --- | --- | --- | --- | --- |\n"
        f"{proof_row}\n\n"
        "## Missing Coverage\n\n- none\n\n"
        f"## Change Gate\n\n{change_gate}\n\n"
        "## Decision Log\n\n"
        "| Date | Decision | Reason | Decider |\n"
        "| --- | --- | --- | --- |\n"
    )


class BehaviorContractCheckTests(unittest.TestCase):
    def test_detects_missing_required_sections(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "bad.md"
            path.write_text("# Bad Contract\n\n## BDD Scenarios\n\n- Given x\n- When y\n- Then z\n", encoding="utf-8")

            errors = behavior_contract_check.check_contract(path)

            self.assertTrue(any("## Product Behavior" in error for error in errors))
            self.assertTrue(any("## Decision Log" in error for error in errors))
            self.assertTrue(any("UNKNOWN_BLOCKING" in error for error in errors))

    def test_detects_invalid_interface_kind(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "bad-kind.md"
            path.write_text(
                "# Bad Contract\n\n"
                "## Product Behavior\n\n"
                "## 中文语义\n\n"
                "**用户承诺：** x\n**核心流程：** y\n**非目标：** z\n\n"
                "## BDD Scenarios\n\n"
                "- Given x\n- When y\n- Then z\n\n"
                "## Interface Binding\n\n"
                "| Interface / Entry | Kind | Source | Expected behavior |\n"
                "| --- | --- | --- | --- |\n"
                "| x | workflow | product | y |\n\n"
                "## Graphify Binding\n\nUNKNOWN_BLOCKING\n\n"
                "## TDD / QA Proof\n\n"
                "## Missing Coverage\n\n"
                "## Change Gate\n\n"
                "## Decision Log\n",
                encoding="utf-8",
            )

            errors = behavior_contract_check.check_contract(path)

            self.assertTrue(any("invalid Interface Binding Kind `workflow`" in error for error in errors))

    def test_detects_unregistered_proof_test_name(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "bad-proof.md"
            path.write_text(
                "# Bad Contract\n\n"
                "## Product Behavior\n\n"
                "## 中文语义\n\n"
                "**用户承诺：** x\n**核心流程：** y\n**非目标：** z\n\n"
                "## BDD Scenarios\n\n"
                "- Given x\n- When y\n- Then z\n\n"
                "## Interface Binding\n\n"
                "| Interface / Entry | Kind | Source | Expected behavior |\n"
                "| --- | --- | --- | --- |\n"
                "| x | API | Graphify | y |\n\n"
                "## Graphify Binding\n\nUNKNOWN_BLOCKING\n\n"
                "## TDD / QA Proof\n\n"
                "| Proof ID | Kind | Artifact / command / event | Pass signal | Status |\n"
                "| --- | --- | --- | --- | --- |\n"
                "| TEST-GHOST | unit | `TestImaginaryContract` | pass | active |\n\n"
                "## Missing Coverage\n\n"
                "## Change Gate\n\n"
                "## Decision Log\n",
                encoding="utf-8",
            )

            errors = behavior_contract_check.check_contract(path)

            self.assertTrue(any("proof references unregistered test `TestImaginaryContract`" in error for error in errors))

    def test_detects_missing_interface_symbol(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "bad-symbol.md"
            path.write_text(
                "# Bad Contract\n\n"
                "## Product Behavior\n\n"
                "## 中文语义\n\n"
                "**用户承诺：** x\n**核心流程：** y\n**非目标：** z\n\n"
                "## BDD Scenarios\n\n"
                "- Given x\n- When y\n- Then z\n\n"
                "## Interface Binding\n\n"
                "| Interface / Entry | Kind | Source | Expected behavior |\n"
                "| --- | --- | --- | --- |\n"
                "| `ImaginaryUiRouter()` | UI | Graphify | y |\n\n"
                "## Graphify Binding\n\nUNKNOWN_BLOCKING\n\n"
                "## TDD / QA Proof\n\n"
                "| Proof ID | Kind | Artifact / command / event | Pass signal | Status |\n"
                "| --- | --- | --- | --- | --- |\n"
                "| TEST-META | unit | `TestMetadataFields` | pass | active |\n\n"
                "## Missing Coverage\n\n"
                "## Change Gate\n\n"
                "## Decision Log\n",
                encoding="utf-8",
            )

            errors = behavior_contract_check.check_contract(path)

            self.assertTrue(any("Interface Binding symbol not found in src/tests `ImaginaryUiRouter()`" in error for error in errors))

    def test_rejects_status_outside_template_enum(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "bad-status.md"
            path.write_text(make_contract(status="Active / Partially Verified"), encoding="utf-8")

            errors = behavior_contract_check.check_contract(path)

            self.assertTrue(any("invalid Status `Active / Partially Verified`" in error for error in errors))

    def test_rejects_english_only_bdd_descriptions(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "english-bdd.md"
            path.write_text(
                make_contract(bdd="- Given a dataset exists\n- When the user searches\n- Then matching rows appear"),
                encoding="utf-8",
            )

            errors = behavior_contract_check.check_contract(path)

            self.assertTrue(any("BDD Given description must contain Chinese" in error for error in errors))
            self.assertTrue(any("BDD When description must contain Chinese" in error for error in errors))
            self.assertTrue(any("BDD Then description must contain Chinese" in error for error in errors))

    def test_rejects_unescaped_pipe_and_empty_pass_signal_in_proof_table(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "bad-proof-shape.md"
            path.write_text(
                make_contract(
                    proof_row="| TEST-BAD | contract | `TestMetadataFields` | ?|legacy identity | |"
                ),
                encoding="utf-8",
            )

            errors = behavior_contract_check.check_contract(path)

            self.assertTrue(any("TDD / QA Proof row must have exactly 5 columns" in error for error in errors))

    def test_rejects_ctest_regex_with_zero_registered_matches(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "zero-match.md"
            path.write_text(
                make_contract(
                    proof_row='| TEST-GHOST | contract | `ctest -R "TestDefinitelyMissing.*"` | pass | active |'
                ),
                encoding="utf-8",
            )

            errors = behavior_contract_check.check_contract(path)

            self.assertTrue(any("ctest -R pattern matches zero registered tests" in error for error in errors))

    def test_rejects_unregistered_test_named_in_change_gate(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "stale-change-gate.md"
            path.write_text(
                make_contract(change_gate="- [x] Re-run `TestDefinitelyMissingFromCtest`."),
                encoding="utf-8",
            )

            errors = behavior_contract_check.check_contract(path)

            self.assertTrue(any("Change Gate references unregistered test" in error for error in errors))

    def test_module_index_and_regression_suite_stay_aligned(self):
        index_text = behavior_contract_check.INDEX_PATH.read_text(encoding="utf-8")
        index_contracts = {
            path.relative_to(ROOT).as_posix()
            for path in behavior_contract_check.module_paths(index_text)
        }
        regression_contracts = {module.contract for module in behavior_regression_suite.MODULES}

        self.assertEqual(index_contracts, regression_contracts)

    def test_module_index_statuses_match_contract_headers(self):
        index_text = behavior_contract_check.INDEX_PATH.read_text(encoding="utf-8")
        mismatches = []
        for line in index_text.splitlines():
            match = re.search(r"`(modules/[^`]+\.md)`", line)
            if not match or not line.startswith("|"):
                continue
            cells = [cell.strip() for cell in line.strip().strip("|").split("|")]
            index_status = cells[-1].casefold()
            contract_path = behavior_contract_check.BEHAVIOR_DIR / match.group(1)
            contract_text = contract_path.read_text(encoding="utf-8")
            contract_status = re.search(
                r"^\*\*Status:\*\*\s*(.*?)\s*$", contract_text, re.MULTILINE
            ).group(1).casefold()
            if index_status != contract_status:
                mismatches.append((match.group(1), index_status, contract_status))

        self.assertEqual(mismatches, [])

    def test_cubase_contract_is_really_registered_with_ctest(self):
        self.assertIn(
            "TestCubaseDirectInsertContract",
            behavior_contract_check.registered_test_names(),
        )

    def test_current_behavior_contracts_pass(self):
        self.assertEqual(behavior_contract_check.check_all(), [])


if __name__ == "__main__":
    unittest.main()

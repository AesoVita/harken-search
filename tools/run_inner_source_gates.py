#!/usr/bin/env python3
"""Run the Inner Source Gates checks locally, in the same order as CI."""

from __future__ import annotations

import os
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
COMPILE_TARGETS = [
    "tools/check_release_compliance.py",
    "tools/qa_provenance_manifest.py",
    "tools/public_repo_manifest.py",
    "tools/cuevi_governance_command.py",
    "tools/cuevi_preflight.py",
    "tools/cuevi_progress_bridge.py",
    "tools/assemble_website_release.py",
    "tools/website_release_page_preview.py",
    "tests/test_stage1_provenance_gate.py",
    "tests/test_release_compliance_contract.py",
    "tests/test_behavior_contract_check.py",
    "tests/test_inner_source_governance.py",
    "tests/test_qa_provenance_manifest.py",
    "tests/test_public_repo_manifest.py",
    "tests/test_cuevi_governance_command.py",
    "tests/test_cuevi_preflight.py",
    "tests/test_cuevi_progress_bridge.py",
    "tools/review_feedback_collector.py",
    "tools/verify_cuevi_feedback_e2e.py",
    "tests/test_review_feedback_collector.py",
    "tests/test_review_feedback_workflow_contract.py",
    "tests/test_aris_card_worktree_gate.py",
    "tests/test_cuevi_feedback_live_e2e_contract.py",
]


def run(command: list[str]) -> int:
    print("+", " ".join(command), flush=True)
    completed = subprocess.run(command, cwd=ROOT, check=False)
    return completed.returncode


def main() -> int:
    python = sys.executable
    if run([python, "-m", "pip", "install", "-r", "requirements-ci.txt"]):
        return 1
    if run([python, "-m", "playwright", "install", "chromium"]):
        return 1
    if run([python, "-m", "py_compile", *COMPILE_TARGETS]):
        return 1
    if run([python, "tests/test_inner_source_governance.py"]):
        return 1
    if run([python, "tools/check_release_compliance.py"]):
        return 1
    if run(
        [
            "node",
            "--test",
            "tests/website/release_page.test.mjs",
            "tests/website/release_catalog_lifecycle.test.mjs",
            "tests/website/website_release_catalog_preview.test.mjs",
        ]
    ):
        return 1
    with tempfile.TemporaryDirectory(prefix="aris-local-gates-") as temp_root:
        env_suffix = [
            "-q",
            "--basetemp",
            os.path.join(temp_root, "pytest"),
        ]
        if run(
            [python, "-m", "pytest", "tests/test_stage1_provenance_gate.py", *env_suffix]
        ):
            return 1
        if run(
            [
                python,
                "-m",
                "pytest",
                "tests/test_release_compliance_contract.py",
                *env_suffix,
            ]
        ):
            return 1
        if run(
            [
                python,
                "-m",
                "pytest",
                "tests/test_cuevi_governance_command.py",
                "tests/test_cuevi_preflight.py",
                "tests/test_cuevi_progress_bridge.py",
                "tests/test_review_feedback_collector.py",
                "tests/test_review_feedback_workflow_contract.py",
                *env_suffix,
            ]
        ):
            return 1
    if run([python, "tests/test_behavior_contract_check.py"]):
        return 1
    if run([python, "tests/test_qa_provenance_manifest.py"]):
        return 1
    if run([python, "-m", "unittest", "tests.test_public_repo_manifest", "-v"]):
        return 1
    if run([python, "-m", "unittest", "tests.test_aris_card_worktree_gate", "-v"]):
        return 1
    if run([python, "-m", "unittest", "tests.test_cuevi_feedback_live_e2e_contract", "-v"]):
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

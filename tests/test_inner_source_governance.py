import json
import re
import unittest
from pathlib import Path

import yaml


ROOT = Path(__file__).resolve().parents[1]
CHECKOUT_SHA = "11bd71901bbe5b1630ceea73d27597364c9af683"  # v4.2.2
SETUP_PYTHON_SHA = "a26af69be951a213d495a4c3e4e4022e16d87065"  # v5.6.0
SETUP_NODE_SHA = "49933ea5288caeca8642d1e84afbd3f7d6820020"  # v4.4.0


def read(relative: str) -> str:
    path = ROOT / relative
    if not path.is_file():
        raise AssertionError(f"required governance file missing: {relative}")
    return path.read_text(encoding="utf-8")


def validate_codeowners(text: str) -> list[str]:
    errors = []
    segment = r"[A-Za-z0-9](?:[A-Za-z0-9-]*[A-Za-z0-9])?"
    owner = re.compile(rf"^@{segment}(?:/{segment})?$")
    forbidden = {"todo", "tbd", "example", "placeholder", "fake"}
    for number, raw in enumerate(text.splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        fields = line.split()
        pattern, owners = fields[0], fields[1:]
        if (
            not owners
            or pattern.startswith(("!", "@"))
            or "[" in pattern
            or "]" in pattern
            or any(not owner.fullmatch(value) for value in owners)
        ):
            errors.append(f"line {number}: malformed ownership rule")
        if any(component.casefold() in forbidden for value in owners for component in value[1:].split("/")):
            errors.append(f"line {number}: unapproved ownership identity")
    return errors


def validate_workflow(text: str) -> list[str]:
    errors = []
    try:
        workflow = yaml.load(text, Loader=yaml.BaseLoader)
    except yaml.YAMLError as error:
        return [f"invalid YAML: {error}"]
    if not isinstance(workflow, dict):
        return ["workflow root must be a mapping"]
    if set(workflow) != {"name", "on", "permissions", "jobs"} or workflow.get("name") != "Inner Source Gates":
        errors.append("workflow top-level keys or name drifted")

    triggers = workflow.get("on")
    if not isinstance(triggers, dict) or set(triggers) != {"pull_request", "push"}:
        errors.append("workflow triggers must be pull_request and push")
    else:
        if triggers.get("pull_request") != "":
            errors.append("pull_request trigger must cover all pull requests")
        if triggers.get("push") != {"branches": ["main"]}:
            errors.append("push trigger must target main")
    if workflow.get("permissions") != {"contents": "read", "pull-requests": "read", "actions": "write"}:
        errors.append("workflow permissions must be contents read, pull-requests read, and actions write")

    jobs = workflow.get("jobs")
    if not isinstance(jobs, dict) or set(jobs) != {"governance-and-release", "cpp-model-tests"}:
        return errors + ["workflow must contain governance-and-release and cpp-model-tests"]
    job = jobs["governance-and-release"]
    extra_job_keys = set(job) - {"name", "runs-on", "timeout-minutes", "steps"}
    import runpy
    required_check = runpy.run_path(str(ROOT / "tools/verify_github_main_protection.py"))["REQUIRED_CHECK"]
    if job.get("name") != required_check:
        errors.append("governance check name must match main protection")
    if extra_job_keys:
        errors.append("governance job keys drifted")
    if "continue-on-error:" in text or re.search(r"(?m)^\s+if:\s+false\s*$", text):
        errors.append("governance workflow must not skip or continue-on-error required gates")
    if job.get("runs-on") != "windows-2022" or job.get("timeout-minutes") != "15":
        errors.append("governance job runner or timeout drifted")

    steps = job.get("steps")
    if not isinstance(steps, list):
        return errors + ["governance job has no steps"]
    named = {step.get("name"): step for step in steps if isinstance(step, dict)}
    required_names = {
        "Check out repository",
        "Fetch verified PR target baseline",
        "Set up Python",
        "Set up Node",
        "Install test runner",
        "Install Chromium",
        "Compile Python gates",
        "Check repository governance",
        "Check source compliance",
        "Generate third-party SBOM",
        "Generate PR SBOM difference",
        "Upload third-party SBOM evidence",
        "Check release page contract",
        "Check Stage 1 provenance",
        "Check release compliance contract",
        "Check Cuevi governance tooling",
        "Check behavior contract",
        "Check QA provenance manifest",
        "Check card worktree delivery gate",
        "Check Cuevi live feedback contract",
    }
    missing = required_names - set(named)
    if missing:
        errors.append("governance job missing required steps: " + ", ".join(sorted(missing)))
    checkout = named.get("Check out repository", {})
    if checkout.get("uses") != f"actions/checkout@{CHECKOUT_SHA}":
        errors.append("checkout action drifted")
    python = named.get("Set up Python", {})
    if python.get("uses") != f"actions/setup-python@{SETUP_PYTHON_SHA}":
        errors.append("setup-python action drifted")
    if python.get("with") != {
        "python-version": "3.13",
        "cache": "pip",
        "cache-dependency-path": "requirements-ci.txt",
    }:
        errors.append("setup-python inputs drifted")
    node = named.get("Set up Node", {})
    if node.get("uses") != f"actions/setup-node@{SETUP_NODE_SHA}":
        errors.append("setup-node action drifted")
    if node.get("with") != {"node-version": "22"}:
        errors.append("setup-node inputs drifted")
    compile_step = " ".join(str(named.get("Compile Python gates", {}).get("run", "")).split())
    for token in (
        "tools/check_release_compliance.py",
        "tools/generate_third_party_sbom.py",
        "tools/verify_github_main_protection.py",
        "tests/test_third_party_sbom.py",
    ):
        if token not in compile_step:
            errors.append(f"compile step missing {token}")
    if "Check open-source PR gate" in named:
        errors.append("legacy open-source PR gate must be retired in favor of Cuevi workflow")
    if "tools/open_source_pr_gate.py" in text or "tests/test_open_source_pr_gate.py" in text:
        errors.append("legacy open-source PR gate references must be retired")
    fetch_step = named.get("Fetch verified PR target baseline", {})
    fetch_run = str(fetch_step.get("run", ""))
    if "BASE_SHA" not in fetch_run or "git fetch --no-tags --depth=1 origin" not in fetch_run:
        errors.append("PR baseline fetch is not fail-closed")
    if fetch_step.get("if") != "github.event_name == 'pull_request'":
        errors.append("PR baseline fetch must be pull-request only")
    sbom_run = str(named.get("Generate third-party SBOM", {}).get("run", ""))
    if "tools/generate_third_party_sbom.py" not in sbom_run:
        errors.append("SBOM generation step drifted")
    diff_run = str(named.get("Generate PR SBOM difference", {}).get("run", ""))
    if "compare-base" not in diff_run or "BASE_SHA" not in diff_run:
        errors.append("SBOM difference step drifted")
    if "git cat-file -e" not in diff_run or '"components": []' not in diff_run:
        errors.append("SBOM difference step must support a first-introduced component ledger")
    if "if (git cat-file -e" in diff_run:
        errors.append("SBOM difference step must not treat empty git cat-file stdout as a missing ledger")
    if "$LASTEXITCODE" not in diff_run:
        errors.append("SBOM difference step must use git cat-file exit code before falling back to an empty ledger")
    upload = named.get("Upload third-party SBOM evidence", {})
    if upload.get("uses") != "actions/upload-artifact@ea165f8d65b6e75b540449e92b4886f43607fa02":
        errors.append("SBOM artifact upload step drifted")
    compliance_run = str(named.get("Check release compliance contract", {}).get("run", ""))
    if "tests/test_third_party_sbom.py" not in compliance_run:
        errors.append("release compliance pytest step missing SBOM tests")
    return errors


class InnerSourceGovernanceContract(unittest.TestCase):
    def test_legacy_cuevi_pr_gate_files_are_retired(self):
        for relative in (
            "tools/open_source_pr_gate.py",
            "tests/test_open_source_pr_gate.py",
            "docs/governance/aris/open_source_pr_gate_policy.json",
        ):
            self.assertFalse((ROOT / relative).exists(), relative)

    def test_contributing_uses_authoritative_process_and_pr_flow(self):
        text = read("CONTRIBUTING.md")
        for token in (
            "MAINTAINERS.md",
            "cmake -S .",
            "cmake --build",
            "ctest --test-dir",
            "python tools/run_inner_source_gates.py",
        ):
            self.assertIn(token, text)
        self.assertRegex(text, r"(?i)do not push directly to `?main`?")
        self.assertRegex(text, r"(?i)pull request.*review")

    def test_security_requires_private_non_invented_reporting(self):
        text = read("SECURITY.md")
        github = read(".github/SECURITY.md")
        self.assertIn("GitHub private vulnerability reporting", text)
        self.assertIn("XD's approved internal security incident", text)
        self.assertIn("harken-support@xd.com", text)
        self.assertRegex(text, r"(?i)do not.*public issue")
        self.assertNotRegex(text, r"(?i)(security@|example\.com|<[^>]+>|TODO|TBD)")
        root_body = text.split("This policy covers Harken Search.", 1)[1]
        github_body = github.split("This policy covers Harken Search.", 1)[1]
        self.assertEqual(root_body, github_body)

    def test_maintainers_defines_roles_domains_and_admin_configuration(self):
        text = read("MAINTAINERS.md")
        for role in (
            "Repository Maintainers",
            "Release Manager",
            "Security Response",
        ):
            self.assertIn(role, text)
        for domain in ("Database", "Search", "Audio", "Indexing", "UI", "Performance"):
            self.assertRegex(text, rf"(?m)^\| {domain} \|")
        for token in ("branch protection", "CODEOWNERS", "repository administrators"):
            self.assertIn(token, text)
        self.assertNotRegex(text, r"(?i)(TODO|TBD|@example|placeholder)")

    def test_pull_request_template_requires_decision_evidence(self):
        text = read(".github/pull_request_template.md")
        for heading in (
            "## Scope",
            "## Risk",
            "## Verification",
            "## Human checks",
            "## Release impact",
            "## Remaining risks",
        ):
            self.assertIn(heading, text)

    def test_optional_codeowners_requires_real_well_formed_owners(self):
        for valid in (
            "src/ @approved-org/audio-reviewers\n",
            "docs/examples/** @approved-org/docs\n",
            "*.md @docs-owner @approved-org/docs\n",
            "src/ @todoist/docs\n",
        ):
            self.assertEqual([], validate_codeowners(valid))
        for invalid in (
            "src/\n",
            "src/ audio-reviewers\n",
            "@approved-org/audio-reviewers\n",
            "src/ @example/team\n",
            "src/ @org/TODO\n",
            "!generated/** @approved-org/docs\n",
            "docs/[ab].md @approved-org/docs\n",
            "src/ @owner.name\n",
            "src/ @owner_name\n",
            "src/ @approved-org/team.name\n",
        ):
            self.assertNotEqual([], validate_codeowners(invalid))
        candidates = (ROOT / ".github" / "CODEOWNERS", ROOT / "CODEOWNERS")
        existing = [path for path in candidates if path.is_file()]
        self.assertLessEqual(len(existing), 1)
        if existing:
            self.assertEqual([], validate_codeowners(existing[0].read_text(encoding="utf-8")))

    def test_ci_dependency_manifest_is_pinned(self):
        requirements = read("requirements-ci.txt")
        self.assertEqual(
            ["pytest==8.4.2", "PyYAML==6.0.3", "cryptography==46.0.3", "playwright==1.60.0",
             "-r tools/requirements-manual.txt"],
            requirements.splitlines(),
        )
        manual = read("tools/requirements-manual.txt")
        self.assertEqual(["PyMuPDF==1.28.2", "Pillow==12.3.0"], manual.splitlines())

    def test_auto_merge_requires_enforced_inner_source_gate(self):
        text = read(".github/workflows/auto-merge.yml")
        self.assertIn("tools/verify_public_repository.py", text)
        self.assertIn("Inner Source Gates / governance-and-release", read("tools/verify_github_main_protection.py"))
        self.assertIn("gh pr merge", text)
        self.assertNotIn("contents: write", text)

    def test_local_pre_pr_audit_covers_inner_source_gates(self):
        text = read("tools/run_inner_source_gates.py")
        for token in (
            '"-m", "pip", "install", "-r", "requirements-ci.txt"',
            '"-m", "playwright", "install", "chromium"',
            '"-m", "py_compile"',
            "tests/test_inner_source_governance.py",
            "tools/check_release_compliance.py",
            '"node"',
            '"--test"',
            "tests/test_stage1_provenance_gate.py",
            "tests/test_release_compliance_contract.py",
            "tests/test_cuevi_governance_command.py",
            "tests/test_behavior_contract_check.py",
            "tests/test_qa_provenance_manifest.py",
            "tests/test_public_repo_manifest.py",
            "tests.test_aris_card_worktree_gate",
            "tests.test_cuevi_feedback_live_e2e_contract",
        ):
            self.assertIn(token, text)
        process = read("docs/process/ARIS_MULTI_DEV_WORKFLOW.md")
        self.assertIn("python tools/run_inner_source_gates.py", process)

    def test_windows_ci_compiles_and_runs_dock_model_tests(self):
        workflow = yaml.load(read(".github/workflows/inner-source-gates.yml"), Loader=yaml.BaseLoader)
        jobs = workflow["jobs"]
        self.assertIn("cpp-model-tests", jobs)
        job = jobs["cpp-model-tests"]
        self.assertEqual("windows-2022", job.get("runs-on"))
        self.assertNotIn("continue-on-error", job)
        rendered = "\n".join(
            " ".join(str(step.get("run", "")).split())
            for step in job.get("steps", [])
            if isinstance(step, dict)
        )
        self.assertFalse(any("if" in step for step in job.get("steps", []) if isinstance(step, dict)))
        for token in (
            "cmake -S . -B build/ci-cpp-model -G \"Visual Studio 17 2022\" -A x64",
            "cmake --build build/ci-cpp-model --config Debug --target TestDockLayoutTree TestDockHitTest TestDockDragSession TestDockNativeCaptionBridge",
            "ctest --test-dir build/ci-cpp-model -C Debug -L dock --no-tests=error --output-on-failure",
        ):
            self.assertIn(token, rendered)
        self.assertNotIn("HarkenSearch", rendered)

    def test_windows_ci_compiles_the_real_installer_script(self):
        workflow = yaml.load(read(".github/workflows/inner-source-gates.yml"), Loader=yaml.BaseLoader)
        job = workflow["jobs"]["cpp-model-tests"]
        rendered = "\n".join(
            " ".join(str(step.get("run", "")).split())
            for step in job.get("steps", [])
            if isinstance(step, dict)
        )
        self.assertIn("Install Inno Setup", " ".join(
            str(step.get("name", "")) for step in job.get("steps", []) if isinstance(step, dict)
        ))
        self.assertIn("choco install innosetup -y --no-progress", rendered)
        self.assertIn("ISCC.exe", rendered)
        self.assertNotIn("--version=6.4.1", rendered)
        self.assertNotIn("--force", rendered)
        self.assertIn("python -m unittest tests.test_installer_iss_compiles -v", rendered)
        self.assertNotIn("skipTest", rendered)

    def test_main_ruleset_template_requires_cpp_model_tests(self):
        ruleset = json.loads(read("docs/governance/github/main-ruleset.template.json"))
        contexts = []
        for rule in ruleset.get("rules", []):
            if rule.get("type") != "required_status_checks":
                continue
            for check in rule.get("parameters", {}).get("required_status_checks", []):
                contexts.append(check.get("context"))
        self.assertIn("Inner Source Gates / governance-and-release", contexts)
        self.assertIn("Inner Source Gates / cpp-model-tests", contexts)

    def test_windows_ci_runs_exact_public_gates(self):
        text = read(".github/workflows/inner-source-gates.yml")
        self.assertEqual([], validate_workflow(text))
        for invalid in (
            text.replace("timeout-minutes: 15", "timeout-minutes: 15\n    permissions: write-all", 1),
            text.replace("timeout-minutes: 15", "timeout-minutes: 15\n    if: false", 1),
            text.replace("# v4.2.2", "# v4.2.2\n        if: false", 1),
            text.replace("run: python -m pip install -r requirements-ci.txt", "run: python -m pip install -r requirements-ci.txt\n        continue-on-error: true", 1),
            text.replace("pull_request:", "pull_request:\n    branches:\n      - feature/*", 1),
            text.replace("pull_request:", "workflow_dispatch:", 1),
            text.replace("governance-and-release:", "wrong-job:", 1),
            text.replace(f"actions/checkout@{CHECKOUT_SHA}", "actions/checkout@v4", 1),
            "# pull_request: contents: read actions/checkout@" + CHECKOUT_SHA + "\non: {workflow_dispatch: {}}\njobs: {}\n",
        ):
            self.assertNotEqual([], validate_workflow(invalid))


if __name__ == "__main__":
    unittest.main()

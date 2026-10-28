"""QA artifacts must describe the exact commit compiled by the workflow."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest

import yaml


ROOT = Path(__file__).resolve().parents[1]
WORKFLOW = ROOT / ".github/workflows/qa-exe.yml"


def qa_steps():
    workflow = yaml.load(WORKFLOW.read_text(encoding="utf-8"), Loader=yaml.BaseLoader)
    return workflow, workflow["jobs"]["build-qa-exe"]["steps"]


class QaWorkflowIdentityTests(unittest.TestCase):
    def test_qa_build_pins_the_event_sha_and_checks_its_branch(self):
        workflow, steps = qa_steps()
        self.assertEqual(workflow["on"]["push"]["branches"], ["qa/current"])
        self.assertEqual(steps[0]["with"]["ref"], "$" + "{{ github.sha }}")
        materialize = steps[1]["run"]
        self.assertIn("refs/heads/qa/current", materialize)
        self.assertIn("git checkout -B qa/current $env:GITHUB_SHA", materialize)
        self.assertIn("git rev-parse HEAD", materialize)
        self.assertNotIn("origin/qa/current", materialize)
        for step in steps[-2:]:
            self.assertIn("$" + "{{ github.sha }}", step["with"]["name"])

    def test_materialization_uses_event_commit_even_if_branch_advances(self):
        _, steps = qa_steps()
        parent = ROOT / "tmp_qa_workflow_tests"
        parent.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=parent) as directory:
            checkout = Path(directory)
            script = checkout / "materialize.ps1"
            script.write_text(steps[1]["run"], encoding="utf-8")
            quoted_script = str(script).replace("'", "''")
            parse = subprocess.run([
                "powershell", "-NoProfile", "-NonInteractive", "-Command",
                f"$errors = $null; [System.Management.Automation.Language.Parser]::ParseFile('{quoted_script}', [ref]$null, [ref]$errors) > $null; if ($errors.Count) {{ exit 1 }}",
            ], capture_output=True, text=True)
            self.assertEqual(parse.returncode, 0, parse.stdout + parse.stderr)

            def git(*args):
                result = subprocess.run(["git", *args], cwd=checkout, capture_output=True, text=True, check=True)
                return result.stdout.strip()

            git("init", "-q")
            source = checkout / "source.txt"
            source.write_text("first", encoding="utf-8")
            git("add", "source.txt")
            git("-c", "user.name=CI", "-c", "user.email=ci@example.invalid", "commit", "-qm", "first")
            event_sha = git("rev-parse", "HEAD")
            source.write_text("later", encoding="utf-8")
            git("add", "source.txt")
            git("-c", "user.name=CI", "-c", "user.email=ci@example.invalid", "commit", "-qm", "later")
            git("checkout", "--detach", event_sha)
            environment = os.environ.copy()
            environment.update(GITHUB_REF="refs/heads/qa/current", GITHUB_SHA=event_sha)
            result = subprocess.run(["powershell", "-NoProfile", "-NonInteractive", "-File", str(script)], cwd=checkout, env=environment, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(git("rev-parse", "HEAD"), event_sha)
            self.assertEqual(git("branch", "--show-current"), "qa/current")
            environment["GITHUB_REF"] = "refs/heads/other"
            rejected = subprocess.run(["powershell", "-NoProfile", "-NonInteractive", "-File", str(script)], cwd=checkout, env=environment, capture_output=True, text=True)
            self.assertNotEqual(rejected.returncode, 0)
            self.assertEqual(git("rev-parse", "HEAD"), event_sha)


if __name__ == "__main__":
    unittest.main()

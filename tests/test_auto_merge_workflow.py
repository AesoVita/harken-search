import unittest
from pathlib import Path

import yaml


ROOT = Path(__file__).resolve().parents[1]
WORKFLOW = ROOT / ".github" / "workflows" / "auto-merge.yml"


class AutoMergeWorkflowTests(unittest.TestCase):
    def test_new_pull_requests_enable_auto_merge_without_legacy_qa_label_gate(self):
        workflow = yaml.load(WORKFLOW.read_text(encoding="utf-8"), Loader=yaml.BaseLoader)

        self.assertEqual(workflow["name"], "Enable PR Auto-merge")
        self.assertEqual(
            workflow["on"]["pull_request_target"]["types"],
            ["opened", "reopened", "ready_for_review", "synchronize", "labeled"],
        )
        self.assertEqual(
            workflow["permissions"],
            {"contents": "read", "pull-requests": "write"},
        )
        job = workflow["jobs"]["enable-auto-merge"]
        condition = job["if"]
        self.assertIn("draft == false", condition)
        self.assertIn("auto-merge-ok", condition)
        self.assertNotIn("head.ref != 'qa/current'", condition)
        self.assertNotIn("qa-approved", condition)

        verify_step = job["steps"][1]
        self.assertEqual(
            verify_step["run"],
            'python tools/verify_github_main_protection.py --repository "$REPOSITORY"',
        )
        self.assertEqual(verify_step["env"]["GH_TOKEN"], "${{ github.token }}")
        self.assertEqual(verify_step["env"]["REPOSITORY"], "${{ github.repository }}")

        merge_step = job["steps"][2]
        self.assertEqual(merge_step["run"], 'gh pr merge "$PR_URL" --auto --merge')
        self.assertEqual(merge_step["env"]["GH_TOKEN"], "${{ github.token }}")
        self.assertEqual(merge_step["env"]["PR_URL"], "${{ github.event.pull_request.html_url }}")


if __name__ == "__main__":
    unittest.main()

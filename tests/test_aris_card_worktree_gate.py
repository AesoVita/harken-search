import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DELIVERY_CHAIN = (
    "Cuevi 卡片 → 占卡回执（tools/cuevi_claim.py）→ 卡片专属分支/worktree → RED/GREEN 与 focused verification → "
    "PR → 独立审计 → qa/current → QA/验收 → 明确合并授权 → 精确候选进入 main"
)


class ArisCardWorktreeGateTests(unittest.TestCase):
    def test_workflow_requires_card_worktree_qa_then_main(self):
        workflow = (ROOT / "docs/process/ARIS_MULTI_DEV_WORKFLOW.md").read_text(encoding="utf-8")
        contributing = (ROOT / "CONTRIBUTING.md").read_text(encoding="utf-8")

        self.assertIn(DELIVERY_CHAIN, workflow)
        self.assertIn("原始任务分支不得直接合入 `main`", workflow)
        self.assertIn("不把 `qa/current` 整支合入 `main`", workflow)
        self.assertNotIn("git checkout main\ngit pull\ngit checkout -b", workflow)
        self.assertIn("卡片专属分支", workflow)
        self.assertIn("qa/current", workflow)
        self.assertIn("main 工作区只读", workflow)
        self.assertNotIn("卡片专属分支", contributing)
        self.assertNotIn("qa/current", contributing)
        self.assertNotIn("main 工作区只读", contributing)

    def test_default_governance_ci_runs_this_gate(self):
        workflow = (ROOT / ".github/workflows/inner-source-gates.yml").read_text(encoding="utf-8")
        self.assertIn("tests/test_aris_card_worktree_gate.py", workflow)
        self.assertIn("Check card worktree delivery gate", workflow)
        self.assertIn("python -m unittest tests.test_aris_card_worktree_gate -v", workflow)


if __name__ == "__main__":
    unittest.main()

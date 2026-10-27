from __future__ import annotations

import importlib.util
from pathlib import Path
import sys
import unittest


SCRIPT_PATH = (
    Path(__file__).resolve().parents[1]
    / ".agents"
    / "skills"
    / "auro-main-slimming"
    / "scripts"
    / "plan_main_slimming.py"
)


def load_module():
    spec = importlib.util.spec_from_file_location("plan_main_slimming", SCRIPT_PATH)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


class PlanMainSlimmingTests(unittest.TestCase):
    @unittest.skipUnless(
        SCRIPT_PATH.is_file(),
        "Optional local auro-main-slimming skill is not distributed with this repository",
    )
    def test_markdown_contains_owner_commands_and_delete_gate(self):
        module = load_module()
        plan = module.SlimmingPlan(
            area="result action menu routing",
            module="ui-orchestration",
            source_symbols=["RenderUI", "Aura::UI::BuildResultDeliveryActions"],
            target_file="src/ResultActionMenu.cpp",
            shared_files=["src/AppState.h"],
            cross_modules=["playback-library-lifecycle"],
        )
        text = module.build_plan_markdown(plan)
        self.assertIn("**Slimming unit:** result action menu routing", text)
        self.assertIn("`RenderUI`", text)
        self.assertIn("py tools/behavior_regression_suite.py --module ui-orchestration --run", text)
        self.assertIn(
            "py tools/behavior_regression_suite.py --module playback-library-lifecycle --run",
            text,
        )
        self.assertIn("Delete gate", text)
        self.assertIn("**Rollback plan:** TODO", text)
        self.assertIn("**Caller-removal evidence:** TODO", text)
        self.assertIn("## Main.cpp Slimming Slice", text)


if __name__ == "__main__":
    unittest.main()

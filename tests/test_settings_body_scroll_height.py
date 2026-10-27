import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
HEADER = ROOT / "src" / "SettingsBodyScrollHeight.h"
CMAKE = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
GATES = (ROOT / ".github" / "workflows" / "inner-source-gates.yml").read_text(encoding="utf-8")
CPP_TEST = ROOT / "tests" / "test_settings_body_scroll_height.cpp"


class SettingsBodyScrollHeightSourceContractTests(unittest.TestCase):
    def test_height_policy_is_extracted_header(self) -> None:
        self.assertTrue(HEADER.is_file())
        text = HEADER.read_text(encoding="utf-8")
        self.assertIn("ShouldUpdateSettingsBodyContentHeight", text)

    def test_settings_dialog_uses_extracted_height_policy(self) -> None:
        self.assertIn('#include "SettingsBodyScrollHeight.h"', MAIN)
        self.assertIn("ShouldUpdateSettingsBodyContentHeight", MAIN)
        self.assertNotIn(
            "nearScrollBottom && measuredBodyH + 2.0f < s_settingsBodyContentH",
            MAIN,
        )

    def test_cpp_behavior_test_is_wired(self) -> None:
        self.assertTrue(CPP_TEST.is_file())
        self.assertIn("TestSettingsBodyScrollHeight", CMAKE)
        self.assertIn("tests/test_settings_body_scroll_height.cpp", CMAKE)

    def test_source_contract_is_wired(self) -> None:
        self.assertIn("SettingsBodyScrollHeightSourceContract", CMAKE)
        self.assertIn("tests/test_settings_body_scroll_height.py", CMAKE)

    def test_source_contract_runs_in_ci(self) -> None:
        self.assertIn("python -m unittest tests.test_settings_body_scroll_height", GATES)

    def test_qa_push_runs_regression(self) -> None:
        workflow = ROOT / ".github" / "workflows" / "settings-scroll-regression.yml"
        self.assertTrue(workflow.is_file())
        text = workflow.read_text(encoding="utf-8")
        self.assertIn("qa/current", text)
        self.assertIn("SettingsBodyScrollHeight", text)


if __name__ == "__main__":
    unittest.main()

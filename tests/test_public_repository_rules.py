import unittest

from tools.verify_public_repository import REQUIRED_CHECKS, evaluate_rules


def rules():
    return [
        {"type": "deletion"}, {"type": "non_fast_forward"},
        {"type": "pull_request", "parameters": {
            "required_review_thread_resolution": True,
            "required_approving_review_count": 0,
        }},
        {"type": "required_status_checks", "parameters": {
            "strict_required_status_checks_policy": True,
            "required_status_checks": [{"context": name} for name in REQUIRED_CHECKS],
        }},
    ]


class PublicRepositoryRulesTests(unittest.TestCase):
    def test_actual_checks_and_thread_resolution_allow_the_current_workflow(self):
        self.assertEqual(evaluate_rules(rules()), [])

    def test_a_missing_product_build_blocks_auto_merge(self):
        value = rules()
        value[-1]["parameters"]["required_status_checks"].pop()
        self.assertTrue(evaluate_rules(value))

    def test_missing_branch_or_review_protection_is_rejected(self):
        for index in (0, 1, 2):
            with self.subTest(index=index):
                value = rules()
                value.pop(index)
                self.assertTrue(evaluate_rules(value))

    def test_checks_must_be_up_to_date_before_merge(self):
        value = rules()
        value[-1]["parameters"]["strict_required_status_checks_policy"] = False
        self.assertTrue(evaluate_rules(value))

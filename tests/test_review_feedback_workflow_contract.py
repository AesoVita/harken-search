from pathlib import Path

import yaml


ROOT = Path(__file__).parents[1]


def test_package_workflow_collects_review_feedback_after_packaging():
    workflow = (ROOT / ".github" / "workflows" / "package-update.yml").read_text(encoding="utf-8")
    assert "pull-requests: read" in workflow
    assert "review_feedback_collector.py" in workflow
    assert "actions/upload-artifact@" in workflow
    assert "review-feedback" in workflow


def test_package_job_can_read_pull_request_feedback():
    workflow = yaml.load(
        (ROOT / ".github" / "workflows" / "package-update.yml").read_text(encoding="utf-8"),
        Loader=yaml.BaseLoader,
    )
    assert workflow["jobs"]["package"]["permissions"]["pull-requests"] == "read"


def test_package_workflow_can_read_check_runs_for_feedback():
    workflow = yaml.load(
        (ROOT / ".github" / "workflows" / "package-update.yml").read_text(encoding="utf-8"),
        Loader=yaml.BaseLoader,
    )
    assert workflow["permissions"]["checks"] == "read"
    assert workflow["jobs"]["package"]["permissions"]["checks"] == "read"


def test_workflow_does_not_apply_or_merge_github_suggestions():
    workflow = (ROOT / ".github" / "workflows" / "package-update.yml").read_text(encoding="utf-8")
    assert "gh pr review" not in workflow
    assert "gh pr merge" not in workflow
    assert "Apply suggestion" not in workflow

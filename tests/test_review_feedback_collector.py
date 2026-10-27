import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parents[1] / "tools"))

from review_feedback_collector import collect_feedback, render_markdown  # noqa: E402
from review_feedback_collector import _category  # noqa: E402


def test_collect_feedback_classifies_and_deduplicates_comments():
    result = collect_feedback(
        repository="xindong/aris",
        pr_number=40,
        head_sha="a" * 40,
        reviews=[
            {"id": 1, "user": {"login": "github-actions[bot]"}, "state": "COMMENTED", "body": ""},
            {"id": 1, "user": {"login": "github-actions[bot]"}, "state": "COMMENTED", "body": ""},
        ],
        comments=[
            {"id": 11, "path": "tests/x.py", "line": 10, "body": "**严重度：中** 测试未覆盖目标分支", "html_url": "https://example.test/11", "user": {"login": "github-actions[bot]"}},
            {"id": 12, "path": "tools/x.py", "line": 20, "body": "建议补充 stderr 输出", "html_url": "https://example.test/12", "user": {"login": "github-actions[bot]"}},
            {"id": 13, "path": "tools/x.py", "line": 21, "body": "secret=ghp_should-not-appear", "html_url": "https://example.test/13", "user": {"login": "human"}},
        ],
        checks=[{"name": "governance-and-release", "status": "COMPLETED", "conclusion": "SUCCESS"}],
    )

    assert result["identity"] == {"repository": "xindong/aris", "prNumber": 40, "headSha": "a" * 40}
    assert len(result["comments"]) == 3
    assert result["comments"][0]["category"] == "must-fix"
    assert result["comments"][1]["category"] == "recommended"
    assert "ghp_should-not-appear" not in json.dumps(result, ensure_ascii=False)
    assert result["checks"][0]["conclusion"] == "SUCCESS"
    assert len(result["reviews"]) == 1
    assert _category("Severity: high") == "must-fix"


def test_render_markdown_is_deterministic_and_explicitly_non_mutating():
    payload = collect_feedback(
        repository="xindong/aris", pr_number=40, head_sha="b" * 40,
        reviews=[], comments=[], checks=[]
    )
    markdown = render_markdown(payload)
    assert "不自动修改 PR" in markdown
    assert "xindong/aris" in markdown
    assert render_markdown(payload) == markdown


def test_review_summary_bodies_are_collected_as_actionable_comments():
    result = collect_feedback(
        repository="xindong/aris", pr_number=45, head_sha="c" * 40,
        reviews=[{"id": 99, "user": {"login": "greptile"}, "state": "COMMENTED", "body": "must fix review summary", "html_url": "https://example.test/review/99"}],
        comments=[], checks=[],
    )
    assert any(item["body"] == "must fix review summary" for item in result["comments"])

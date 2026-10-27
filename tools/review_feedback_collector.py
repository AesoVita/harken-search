"""Collect GitHub PR feedback after a package/build action without mutating the PR."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
from pathlib import Path
from typing import Any


SECRET_RE = re.compile(r"(?i)(gh[pousr]_[A-Za-z0-9_\-]{8,}|github_pat_[A-Za-z0-9_\-]{8,}|sk-[A-Za-z0-9_\-]{8,})")


def _safe_text(value: Any) -> str:
    text = str(value or "")
    return SECRET_RE.sub("[REDACTED]", text)


def _category(body: str) -> str:
    lowered = body.lower()
    if any(token in lowered for token in ("severity: high", "severity: critical", "must fix", "blocker", "需作者处理", "严重度：高", "严重度:高")):
        return "must-fix"
    if any(token in body for token in ("严重度：中", "需作者处理", "必须", "阻断")):
        return "must-fix"
    if any(token in lowered for token in ("security", "ed25519", "trust boundary")) or "信任链" in body:
        return "follow-up-security"
    if any(token in body for token in ("建议", "建议：", "顺带", "可改进")):
        return "recommended"
    return "review-note"


def _comment_key(comment: dict[str, Any]) -> tuple[Any, ...]:
    return (comment.get("id"), comment.get("html_url"), comment.get("path"), comment.get("line"), comment.get("body"))


def collect_feedback(
    *, repository: str, pr_number: int | None, head_sha: str, reviews: list[dict[str, Any]],
    comments: list[dict[str, Any]], checks: list[dict[str, Any]],
) -> dict[str, Any]:
    seen: set[tuple[Any, ...]] = set()
    normalized_comments: list[dict[str, Any]] = []
    for raw in comments:
        key = _comment_key(raw)
        if key in seen:
            continue
        seen.add(key)
        body = _safe_text(raw.get("body"))
        normalized_comments.append({
            "id": raw.get("id"),
            "category": _category(body),
            "author": _safe_text((raw.get("user") or {}).get("login")),
            "path": _safe_text(raw.get("path")),
            "line": raw.get("line"),
            "body": body,
            "url": _safe_text(raw.get("html_url")),
        })
    for raw in reviews:
        body = _safe_text(raw.get("body"))
        if not body.strip():
            continue
        comment = {
            "id": raw.get("id"),
            "html_url": raw.get("html_url"),
            "path": "",
            "line": None,
            "body": body,
            "user": raw.get("user"),
        }
        key = _comment_key(comment)
        if key not in seen:
            seen.add(key)
            normalized_comments.append({
                "id": comment.get("id"),
                "category": _category(body),
                "author": _safe_text((comment.get("user") or {}).get("login")),
                "path": "",
                "line": None,
                "body": body,
                "url": _safe_text(comment.get("html_url")),
            })
    normalized_comments.sort(key=lambda item: (item["category"], item["path"], item["line"] or 0, item["id"] or 0))
    normalized_reviews: list[dict[str, Any]] = []
    review_seen: set[tuple[Any, ...]] = set()
    for item in reviews:
        author = _safe_text((item.get("user") or {}).get("login"))
        state = _safe_text(item.get("state"))
        key = (item.get("id"),) if item.get("id") is not None else (item.get("html_url"), item.get("body"), state, author)
        if key in review_seen:
            continue
        review_seen.add(key)
        normalized_reviews.append({"id": item.get("id"), "author": author, "state": state})
    normalized_reviews.sort(key=lambda item: (item["state"], item["id"] or 0))
    normalized_checks = [
        {"name": _safe_text(item.get("name")), "status": _safe_text(item.get("status")), "conclusion": _safe_text(item.get("conclusion"))}
        for item in checks
    ]
    normalized_checks.sort(key=lambda item: item["name"])
    identity = {"repository": repository, "prNumber": pr_number, "headSha": head_sha}
    digest = hashlib.sha256(json.dumps({"identity": identity, "comments": normalized_comments, "reviews": normalized_reviews, "checks": normalized_checks}, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")).hexdigest()
    return {
        "schemaVersion": 1,
        "identity": identity,
        "mutatingActions": {"applySuggestion": False, "approve": False, "merge": False},
        "digest": digest,
        "comments": normalized_comments,
        "reviews": normalized_reviews,
        "checks": normalized_checks,
    }


def render_markdown(payload: dict[str, Any]) -> str:
    identity = payload["identity"]
    lines = [
        "# GitHub PR 建议整理报告", "", "本报告只收集和分类建议，不自动修改 PR、不自动批准、不自动合并。", "",
        f"- 仓库：`{identity['repository']}`", f"- PR：`{identity['prNumber']}`", f"- head SHA：`{identity['headSha']}`", f"- 报告摘要 SHA：`{payload['digest']}`", "",
        "## 行内建议", "",
    ]
    if not payload["comments"]:
        lines.append("没有读取到行内建议。")
    for item in payload["comments"]:
        lines.extend([f"### {item['category']} — {item['path']}:{item['line'] or '?'}", f"- 作者：{item['author']}", f"- 链接：{item['url']}", "", item["body"], ""])
    lines.extend(["## 检查结果", ""])
    for check in payload["checks"]:
        lines.append(f"- `{check['name']}`：{check['status']} / {check['conclusion']}")
    return "\n".join(lines).rstrip() + "\n"


def _gh_json(path: str, *, paginate: bool = False) -> Any:
    command = ["gh", "api"]
    if paginate:
        command.extend(["--paginate", "--slurp"])
    command.append(path)
    result = subprocess.run(command, check=False, capture_output=True, text=True, encoding="utf-8")
    if result.returncode != 0:
        detail = result.stderr.strip() or result.stdout.strip() or f"gh api exited with {result.returncode}"
        raise RuntimeError(detail)
    value = json.loads(result.stdout)
    if paginate:
        if not isinstance(value, list):
            raise RuntimeError("gh api pagination returned an unexpected response")
        flattened: list[Any] = []
        for page in value:
            if isinstance(page, list):
                flattened.extend(page)
            elif isinstance(page, dict) and isinstance(page.get("check_runs"), list):
                flattened.extend(page["check_runs"])
            else:
                raise RuntimeError("gh api pagination returned an unexpected page")
        return flattened
    return value


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repository", required=True)
    parser.add_argument("--pr-number", type=int, default=None)
    parser.add_argument("--head-sha", required=False, default=None)
    parser.add_argument("--output-json", required=True)
    parser.add_argument("--output-markdown", required=True)
    args = parser.parse_args()
    if args.pr_number is None:
        reviews: list[dict[str, Any]] = []
        comments: list[dict[str, Any]] = []
        checks: list[dict[str, Any]] = []
        head_sha = args.head_sha or os.environ.get("GITHUB_SHA")
        if not isinstance(head_sha, str) or not head_sha:
            raise ValueError("--head-sha or GITHUB_SHA is required when no PR is selected")
    else:
        prefix = f"repos/{args.repository}/pulls/{args.pr_number}"
        pr_payload = _gh_json(prefix)
        actual_head_sha = (pr_payload.get("head") or {}).get("sha") if isinstance(pr_payload, dict) else None
        if not isinstance(actual_head_sha, str) or not actual_head_sha:
            raise ValueError("selected PR has no resolvable head SHA")
        if args.head_sha and args.head_sha != actual_head_sha:
            raise ValueError("provided head SHA does not match selected PR head SHA")
        head_sha = actual_head_sha
        reviews = _gh_json(prefix + "/reviews?per_page=100", paginate=True)
        comments = _gh_json(prefix + "/comments?per_page=100", paginate=True)
        comments.extend(_gh_json(f"repos/{args.repository}/issues/{args.pr_number}/comments?per_page=100", paginate=True))
        checks = _gh_json(f"repos/{args.repository}/commits/{head_sha}/check-runs?per_page=100", paginate=True)
    payload = collect_feedback(repository=args.repository, pr_number=args.pr_number, head_sha=head_sha, reviews=reviews, comments=comments, checks=checks)
    json_path = Path(args.output_json)
    markdown_path = Path(args.output_markdown)
    json_path.parent.mkdir(parents=True, exist_ok=True)
    json_path.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    markdown_path.write_text(render_markdown(payload), encoding="utf-8")
    print(json.dumps({"status": "ok", "digest": payload["digest"], "commentCount": len(payload["comments"])}, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

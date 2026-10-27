#!/usr/bin/env python3
"""Read-only verification for Aris main-branch protection."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from typing import Any

REQUIRED_CHECK = "Inner Source Gates / governance-and-release"
MAIN_REF_SELECTORS = {"refs/heads/main", "main"}


def github_json(path: str, repository: str) -> Any:
    completed = subprocess.run(
        ["gh", "api", path.replace("{repo}", repository)],
        check=False,
        capture_output=True,
        text=True,
    )
    if completed.returncode:
        raise RuntimeError(completed.stderr.strip() or "GitHub API request failed")
    return json.loads(completed.stdout)


def _as_list(value: Any) -> list[Any]:
    if value is None:
        return []
    if isinstance(value, list):
        return value
    return [value]


def _rule_map(rules: Any) -> dict[str, dict[str, Any]]:
    mapped: dict[str, dict[str, Any]] = {}
    for rule in _as_list(rules):
        if isinstance(rule, dict) and rule.get("type"):
            mapped[str(rule["type"])] = rule
    return mapped


def ruleset_targets_main(ruleset: dict[str, Any], default_branch: str) -> bool:
    if ruleset.get("target") not in {None, "branch"}:
        return False
    conditions = ruleset.get("conditions") or {}
    ref_name = conditions.get("ref_name") or {}
    include = {str(item) for item in _as_list(ref_name.get("include"))}
    exclude = {str(item) for item in _as_list(ref_name.get("exclude"))}
    selectors = set(MAIN_REF_SELECTORS)
    if default_branch == "main":
        selectors.add("~DEFAULT_BRANCH")
    if exclude & selectors:
        return False
    if not include:
        return False
    return bool(include & selectors)


def ruleset_has_required_check(ruleset: dict[str, Any]) -> bool:
    rule = _rule_map(ruleset.get("rules")).get("required_status_checks", {})
    parameters = rule.get("parameters") or {}
    for check in _as_list(parameters.get("required_status_checks")):
        if isinstance(check, dict) and check.get("context") == REQUIRED_CHECK:
            return True
        if check == REQUIRED_CHECK:
            return True
    return False


def _truthy(value: Any) -> bool:
    return value in {True, "true"}


def ruleset_has_required_reviews(ruleset: dict[str, Any]) -> bool:
    rule = _rule_map(ruleset.get("rules")).get("pull_request", {})
    parameters = rule.get("parameters") or {}
    try:
        required = int(parameters.get("required_approving_review_count") or 0)
    except (TypeError, ValueError):
        required = 0
    return (
        required >= 1
        and _truthy(parameters.get("dismiss_stale_reviews_on_push"))
        and _truthy(parameters.get("require_last_push_approval"))
        and _truthy(parameters.get("required_review_thread_resolution"))
    )


def ruleset_blocks_force_and_delete(ruleset: dict[str, Any]) -> bool:
    types = set(_rule_map(ruleset.get("rules")))
    return "deletion" in types and "non_fast_forward" in types


def ruleset_is_unbypassable(ruleset: dict[str, Any]) -> bool:
    return not _as_list(ruleset.get("bypass_actors"))


def ruleset_is_complete_main_gate(ruleset: dict[str, Any], default_branch: str) -> bool:
    return (
        ruleset.get("enforcement") == "active"
        and ruleset_targets_main(ruleset, default_branch)
        and ruleset_is_unbypassable(ruleset)
        and ruleset_has_required_check(ruleset)
        and ruleset_has_required_reviews(ruleset)
        and ruleset_blocks_force_and_delete(ruleset)
    )


def evaluate_rulesets(rulesets: Any, default_branch: str) -> list[str]:
    active = [
        ruleset
        for ruleset in _as_list(rulesets)
        if isinstance(ruleset, dict) and ruleset.get("enforcement") == "active" and ruleset_targets_main(ruleset, default_branch)
    ]
    if not active:
        return ["main has no active repository ruleset covering main"]
    if any(ruleset_is_complete_main_gate(ruleset, default_branch) for ruleset in active):
        return []
    issues: list[str] = []
    if not any(ruleset_has_required_check(ruleset) and ruleset_is_unbypassable(ruleset) for ruleset in active):
        issues.append(f"no single active main ruleset enforces required check: {REQUIRED_CHECK}")
    if not any(ruleset_has_required_reviews(ruleset) and ruleset_is_unbypassable(ruleset) for ruleset in active):
        issues.append(
            "no single active main ruleset requires pull request review with stale dismissal, last-push approval, and review thread resolution"
        )
    if not any(ruleset_blocks_force_and_delete(ruleset) and ruleset_is_unbypassable(ruleset) for ruleset in active):
        issues.append("no single active main ruleset forbids force-push and branch deletion")
    if not any(ruleset_is_unbypassable(ruleset) for ruleset in active):
        issues.append("active main ruleset allows bypass actors")
    if not issues:
        issues.append("no single active main ruleset simultaneously enforces all required protections")
    return issues


def evaluate_branch_protection(branch: Any) -> list[str]:
    if not isinstance(branch, dict):
        return ["main branch protection payload is unreadable"]
    issues: list[str] = []
    reviews = branch.get("required_pull_request_reviews") or {}
    if not isinstance(reviews, dict) or int(reviews.get("required_approving_review_count") or 0) < 1:
        issues.append("main does not require pull request review")
    if not reviews.get("dismiss_stale_reviews"):
        issues.append("main does not dismiss stale reviews")
    if not reviews.get("require_last_push_approval"):
        issues.append("main does not require last-push approval")
    conversation_resolution = branch.get("required_conversation_resolution") or {}
    if not isinstance(conversation_resolution, dict) or not conversation_resolution.get("enabled"):
        issues.append("main does not require review thread resolution")
    status = branch.get("required_status_checks") or {}
    contexts = set(_as_list(status.get("contexts")))
    checks = status.get("checks") or []
    for check in _as_list(checks):
        if isinstance(check, dict) and check.get("context"):
            contexts.add(str(check["context"]))
    if REQUIRED_CHECK not in contexts:
        issues.append(f"main does not enforce required check: {REQUIRED_CHECK}")
    if branch.get("allow_force_pushes") not in {False, "false", None} and branch.get("allow_force_pushes", True):
        if branch.get("allow_force_pushes") is True or branch.get("allow_force_pushes") == "true":
            issues.append("main still allows force-push")
    if branch.get("allow_deletions") in {True, "true"}:
        issues.append("main still allows branch deletion")
    if branch.get("enforce_admins") in {False, "false"}:
        issues.append("main protection can be bypassed by administrators")
    return issues


def hydrate_rulesets(repository: str, rulesets: Any) -> list[Any]:
    """Replace repository-ruleset list summaries with their detail payloads."""
    hydrated: list[Any] = []
    for ruleset in _as_list(rulesets):
        if not isinstance(ruleset, dict) or ruleset.get("id") is None:
            hydrated.append(ruleset)
            continue
        hydrated.append(github_json(f"repos/{{repo}}/rulesets/{ruleset['id']}", repository))
    return hydrated


def verify(
    repository: str,
    *,
    rulesets: Any | None = None,
    branch: Any | None = None,
    default_branch: str | None = None,
) -> list[str]:
    if default_branch is None:
        repository_payload = github_json("repos/{repo}", repository)
        default_branch = str(repository_payload.get("default_branch") or "")
    if not default_branch:
        return ["repository default branch is unreadable"]
    if rulesets is None:
        rulesets = hydrate_rulesets(
            repository,
            github_json("repos/{repo}/rulesets", repository),
        )
    if branch is None:
        try:
            branch = github_json("repos/{repo}/branches/main/protection", repository)
        except RuntimeError:
            branch = {}
    ruleset_issues = evaluate_rulesets(rulesets, default_branch)
    if not ruleset_issues:
        return []
    branch_issues = evaluate_branch_protection(branch)
    if not branch_issues:
        return []
    return sorted(set(ruleset_issues + branch_issues))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Read-only GitHub main protection verifier.")
    parser.add_argument("--repository", required=True, help="owner/name; no remote settings are changed")
    args = parser.parse_args(argv)
    try:
        issues = verify(args.repository)
    except (RuntimeError, json.JSONDecodeError) as error:
        print(f"MAIN BRANCH PROTECTION UNVERIFIED: {error}")
        return 1
    if issues:
        print("MAIN BRANCH PROTECTION FAILED")
        for issue in issues:
            print(f" - {issue}")
        return 1
    print("MAIN BRANCH PROTECTION PASSED")
    print(f"main enforces {REQUIRED_CHECK}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

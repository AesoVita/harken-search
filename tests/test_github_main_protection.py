from __future__ import annotations

import importlib.util
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
REQUIRED_CHECK = "Inner Source Gates / governance-and-release"


def load_verifier():
    spec = importlib.util.spec_from_file_location(
        "verify_github_main_protection",
        ROOT / "tools" / "verify_github_main_protection.py",
    )
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


def active_main_ruleset(**overrides):
    payload = {
        "name": "Aris main public contribution gate",
        "target": "branch",
        "enforcement": "active",
        "conditions": {"ref_name": {"include": ["~DEFAULT_BRANCH"], "exclude": []}},
        "rules": [
            {"type": "deletion"},
            {"type": "non_fast_forward"},
            {
                "type": "pull_request",
                "parameters": {
                    "dismiss_stale_reviews_on_push": True,
                    "require_last_push_approval": True,
                    "required_approving_review_count": 1,
                    "required_review_thread_resolution": True,
                },
            },
            {
                "type": "required_status_checks",
                "parameters": {
                    "required_status_checks": [{"context": REQUIRED_CHECK}],
                },
            },
        ],
        "bypass_actors": [],
    }
    payload.update(overrides)
    return payload


def test_disabled_ruleset_is_not_treated_as_protection():
    verifier = load_verifier()
    issues = verifier.verify(
        "xindong/aris",
        rulesets=[active_main_ruleset(enforcement="disabled")],
        default_branch="main",
        branch={
            "required_pull_request_reviews": {"required_approving_review_count": 1},
            "required_status_checks": {"contexts": [REQUIRED_CHECK]},
        },
    )
    assert any("no active repository ruleset covering main" in issue for issue in issues)


def test_wrong_target_ruleset_is_not_treated_as_protection():
    verifier = load_verifier()
    issues = verifier.verify(
        "xindong/aris",
        rulesets=[active_main_ruleset(conditions={"ref_name": {"include": ["refs/heads/qa/current"]}})],
        default_branch="main",
        branch={},
    )
    assert any("no active repository ruleset covering main" in issue for issue in issues)


def test_bypass_actors_fail_closed():
    verifier = load_verifier()
    issues = verifier.verify(
        "xindong/aris",
        rulesets=[active_main_ruleset(bypass_actors=[{"actor_id": 1, "actor_type": "Team"}])],
        default_branch="main",
        branch={},
    )
    assert any("allows bypass actors" in issue for issue in issues)


def test_active_unbypassable_main_ruleset_passes():
    verifier = load_verifier()
    assert verifier.verify("xindong/aris", rulesets=[active_main_ruleset()], branch={}, default_branch="main") == []


def _ruleset_without_review_constraint(key: str):
    payload = active_main_ruleset()
    for rule in payload["rules"]:
        if rule.get("type") == "pull_request":
            rule["parameters"][key] = False
    return payload


def test_missing_last_push_approval_is_not_complete_protection():
    verifier = load_verifier()
    issues = verifier.verify(
        "xindong/aris",
        rulesets=[_ruleset_without_review_constraint("require_last_push_approval")],
        default_branch="main",
        branch={},
    )
    assert any("last-push approval" in issue for issue in issues)


def test_missing_review_thread_resolution_is_not_complete_protection():
    verifier = load_verifier()
    issues = verifier.verify(
        "xindong/aris",
        rulesets=[_ruleset_without_review_constraint("required_review_thread_resolution")],
        default_branch="main",
        branch={},
    )
    assert any("review thread resolution" in issue for issue in issues)


def legacy_main_protection():
    return {
        "required_pull_request_reviews": {
            "required_approving_review_count": 1,
            "dismiss_stale_reviews": True,
            "require_last_push_approval": True,
        },
        "required_conversation_resolution": {"enabled": True},
        "required_status_checks": {"contexts": [REQUIRED_CHECK]},
        "allow_force_pushes": False,
        "allow_deletions": False,
        "enforce_admins": True,
    }


def test_legacy_protection_requires_last_push_approval():
    verifier = load_verifier()
    branch = legacy_main_protection()
    branch["required_pull_request_reviews"]["require_last_push_approval"] = False

    issues = verifier.evaluate_branch_protection(branch)

    assert any("last-push approval" in issue for issue in issues)


def test_legacy_protection_requires_review_thread_resolution():
    verifier = load_verifier()
    branch = legacy_main_protection()
    branch["required_conversation_resolution"] = {"enabled": False}

    issues = verifier.evaluate_branch_protection(branch)

    assert any("review thread resolution" in issue for issue in issues)


def test_default_branch_selector_does_not_cover_main_when_default_branch_differs():
    verifier = load_verifier()
    issues = verifier.verify(
        "xindong/aris",
        rulesets=[active_main_ruleset()],
        branch={},
        default_branch="develop",
    )
    assert any("no active repository ruleset covering main" in issue for issue in issues)


def test_mixed_rulesets_cannot_combine_protections_across_entries():
    verifier = load_verifier()
    complete_but_bypassable = active_main_ruleset(
        bypass_actors=[{"actor_id": 1, "actor_type": "Team"}]
    )
    empty_unbypassable = active_main_ruleset(rules=[], bypass_actors=[])
    issues = verifier.evaluate_rulesets([complete_but_bypassable, empty_unbypassable], "main")
    assert issues
    assert any("no single active main ruleset" in issue for issue in issues)


def test_remote_ruleset_summaries_are_hydrated_before_evaluation(monkeypatch):
    verifier = load_verifier()
    summary = {
        "id": 18613987,
        "name": "Aris main public contribution gate",
        "target": "branch",
        "enforcement": "active",
    }
    detail = active_main_ruleset(id=18613987)
    requests = []

    def fake_github_json(path, repository):
        requests.append((path, repository))
        if path == "repos/{repo}/rulesets":
            return [summary]
        if path == "repos/{repo}/rulesets/18613987":
            return detail
        if path == "repos/{repo}":
            return {"default_branch": "main"}
        raise AssertionError(f"unexpected GitHub API path: {path}")

    monkeypatch.setattr(verifier, "github_json", fake_github_json)

    assert verifier.verify("xindong/aris", branch={}) == []
    assert requests == [
        ("repos/{repo}", "xindong/aris"),
        ("repos/{repo}/rulesets", "xindong/aris"),
        ("repos/{repo}/rulesets/18613987", "xindong/aris"),
    ]

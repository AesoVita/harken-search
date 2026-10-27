# Aris Behavior Contracts

This directory localizes BDD/TDD for Aris modules.

The goal is to keep behavior out of transient discussion and bind it to:

- product workflow semantics
- Given / When / Then acceptance behavior
- interface entries
- Graphify impact and audit evidence
- tests, QA harness commands, and diagnostics
- explicit missing coverage

## Files

- `module-index.md`: module coverage table and gate status
- `regression-suite.md`: full BDD/TDD regression suite mapping
- `acceptance-checklist.md`: product-signoff checklist by module
- `manual-acceptance-checklist.md`: pure manual signoff checklist for software-only validation
- `modules/*.md`: one behavior contract per module
- Keep new contracts in `modules/*.md` using the same sections as existing files.

## Gate

For any bugfix, feature, optimization, or refactor that touches a covered module:

1. Read the module contract before editing.
2. Run `py tools/graphify_auto_refresh.py --once` after edits.
3. Review module-relevant `UNKNOWN_BLOCKING` and `POSSIBLE_DYNAMIC` entries.
4. Run the module's listed TDD / QA proof.
5. Use `acceptance-checklist.md` when writing module or release acceptance.
6. Use `manual-acceptance-checklist.md` when the validator will only operate the real app and will not run commands.
7. Update the contract when behavior, interface binding, or proof mapping changes.
8. Keep `## 中文语义` in sync with the behavior change; every contract must include `用户承诺`, `核心流程`, and `非目标`.

If a touched module has no contract, copy an existing `modules/*.md` contract and fill the same sections before reporting completion.

## Regression Entry

Use the local suite tool to turn contracts into test selection:

```powershell
py tools/behavior_regression_suite.py --list
py tools/behavior_regression_suite.py --module search-query --run
py tools/behavior_regression_suite.py --all --run
```

For signoff wording, use `docs/behavior/acceptance-checklist.md` instead of inventing a one-off acceptance summary from memory.

For manual product validation without command-line steps, use `docs/behavior/manual-acceptance-checklist.md`.

If the selected suite is not run before completion, report `INCOMPLETE_VERIFICATION`.

# UI Orchestration Behavior Contract

## Header

**ID:** BDD-UI-ORCHESTRATION
**Module:** UI Orchestration
**Status:** Active
**Priority:** P1
**Owner:** Engineering
**Created:** 2026-06-04
**Last Updated:** 2026-07-11

## Product Behavior

**User promise:** Keyboard shortcuts, sidebar navigation, result selection, context menus, and detail panels act on the same current file and the same current workspace state instead of drifting apart.
**Primary workflow:** User navigates results, switches tabs or sidebars, invokes shortcuts or context menus, and expects the selected file, pending audio target, delivery actions, and visible side panels to stay in sync.
**Non-goals:** This contract does not define playback decode internals, DB-switch recovery internals, or visual polish of every individual widget.

## 中文语义

**用户承诺：** 用户用键盘、侧边栏、右键菜单、结果列表和详情面板操作时，Aris 必须始终围绕同一个当前选择和同一个当前工作区行动，不能出现“看见的是 A，快捷键却作用在 B”的状态漂移。
**核心流程：** 用户在结果列表里选中文件、切换标签或侧边栏、触发快捷键或右键菜单、查看详情面板和交付动作时，`selectedResultIndex`、pending audio target、delivery actions 和可见面板必须同步收敛到同一语义目标。
**非目标：** 这份合约不定义底层播放解码细节、不定义切库恢复内部流程，也不试图覆盖每个控件的视觉 polish。

## BDD Scenarios

### Happy Path

- Given 已满足以下前置条件：the user selects a result and then uses keyboard shortcuts, sidebar navigation, or result action menus
- When 执行以下操作：Aris routes those UI actions through the current workspace state
- Then 应得到以下结果：the selected file, pending audio target, visible detail panel, and delivery/menu actions all refer to the same item

### Edge Cases

- Given 已满足以下前置条件：the user saves and restores a search workspace or switches sidebar sections
- When 执行以下操作：Aris reapplies the workspace snapshot
- Then 应得到以下结果：search text, folder filter, breadcrumb fallback, selection focus, and refresh intent restore coherently instead of partially applying stale state
- Given 已满足以下前置条件：the user customizes shortcuts or uses presets
- When 执行以下操作：Aris resolves keybindings for navigation and actions
- Then 应得到以下结果：navigation shortcuts do not collide with pitch/history or other claimed bindings, and the effective shortcut set remains internally consistent
- Given 已满足以下前置条件：the user opens a context menu or delivery action menu from the current selection
- When 执行以下操作：Aris builds the action model
- Then 应得到以下结果：only actions valid for the current selection and channel layout are visible/enabled, with no hidden dependency on stale multi-select or old row state

### Failure / Recovery

- Given 已满足以下前置条件：the current selection becomes invalid because of tab reset, DB switch, or out-of-range state
- When 执行以下操作：the UI pumps the next orchestration frame
- Then 应得到以下结果：Aris clears stale selection/pending-target state before routing shortcuts, menus, or detail panels to that row
- Given 已满足以下前置条件：a shortcut rebinding conflicts with an existing command
- When 执行以下操作：the user records the new binding
- Then 应得到以下结果：Aris rejects the conflict instead of silently making two commands compete for the same gesture

### Non-Functional

- Given 已满足以下前置条件：the first UI frame after filter, tab, or sidebar changes
- When 执行以下操作：Aris renders orchestration surfaces
- Then 应得到以下结果：first-paint routing keeps the interface responsive by deferring non-critical follow-up work such as count/facet refresh, instead of blocking the same frame on full recomputation
- Given 已满足以下前置条件：a 100w+ library and the user switches from file browser or playlist into the database view
- When 执行以下操作：the top navigation submits the database refresh
- Then 应得到以下结果：the first paint uses a rows-first request and skips the automatic deferred full-count follow-up for that navigation request, so a million-row count cannot monopolize the post-click frames
- Given 已满足以下前置条件：the UI emits selection, navigation, and action events
- When 执行以下操作：diagnostics or QA capture user interaction history
- Then 应得到以下结果：action logging preserves the semantic order of search, filter, select, play, navigate, edit, export, shortcut, and UI events for later verification

## Interface Binding

| Interface / Entry | Kind | Source | Expected behavior |
| --- | --- | --- | --- |
| `RenderUI()` | UI | Graphify | Top-level UI orchestration converges sidebars, selection, panels, and actions in one frame loop |
| `ProcessKeybindings(AppState& state)` | UI | Graphify + source scan | Keyboard actions resolve against the current selection/workspace without shortcut collisions |
| `RenderSidebarPanelContent(AppState& state, const char* idScope)` | UI | Graphify + tests | Sidebar section order and navigation state remain stable across save/restore and drag reorder |
| `RenderFileInfoPanelContent(AppState& state, float panelWidth)` | UI | Graphify + tests | Detail panel reflects the current selected/pending audio target rather than stale row state |
| `Aura::UI::BuildResultDeliveryActions(...)` | API | source scan + tests | Result action menu visibility/enabled state matches the current selection and channel layout |
| `AppState::beginAudioTargetFromSearchResult()` | API | tests | Pending audio target and selection converge on the chosen search result |
| `selectedResultIndex` | API | source scan + tests | Current row selection is the authority for list/detail/action routing until explicitly cleared |
| `UserActionLog::log()` | diagnostics | tests | UI interaction history is exported in semantic order for QA/debug verification |

## Graphify Binding

**Source graph:** `graphify-out/main/interface-call-graph.json` and `graphify-out/main/audit-call-graph.json`
**Impact target(s):** `RenderUI`, `ProcessKeybindings`, `RenderSidebarPanelContent`, `RenderFileInfoPanelContent`, `BuildResultDeliveryActions`, `beginAudioTargetFromSearchResult`
**Audit classification requirement:** UI orchestration unknowns must be reviewed before completion because main.cpp routing drift can silently break multiple modules at once.
**Dynamic calls:** review menu callbacks, shortcut handlers, and deferred sidebar/detail-panel refresh paths.
**Unknown calls:** review main.cpp routing unknowns plus AppState/UI helper unknowns that touch current selection or workspace restore.

## TDD / QA Proof

| Proof ID | Kind | Artifact / command / event | Pass signal | Status |
| --- | --- | --- | --- | --- |
| TEST-SEARCH-WORKSPACE | integration | `TestSearchWorkspace` | workspace snapshot restore and pending audio target semantics pass | active |
| TEST-SIDEBAR-ORDER | unit | `TestSidebarOrder` | sidebar parse/reorder state remains stable | active |
| TEST-RESULT-ACTION-MENU | unit | `TestResultActionMenuModel` | result delivery actions match selection/channel semantics | active |
| TEST-SHORTCUT-PRESETS | unit | `TestShortcutPresets` | shortcut presets avoid conflicting bare-arrow navigation semantics | active |
| TEST-SWITCH-SELECTION-RESET | integration | `TestSwitchDatabaseBoundary` | invalid selection is cleared before post-switch UI routing continues | active |
| TEST-DATABASE-NAV-NONBLOCKING | source contract | `TestDatabaseNavNonblockingContract` | browser/playlist -> database navigation skips the million-row deferred count follow-up | active |
| DIAG-USER-ACTION-LOG | diagnostics | `TestUserActionLog`, exported `user_actions.log` timeline | semantic UI action categories are recorded in order | active |

## Missing Coverage

- Missing proof: full end-to-end right-click menu plus keyboard shortcut path on a live UI surface is still not asserted automatically.
- Missing proof: one contract test that ties sidebar, result list, file info panel, and delivery menu together in the same runtime flow.
- Residual gap: tab-switch visual continuity and focus affordance are still human-verified.
- Human-only verification: subtle focus/hover/selection polish across dense multi-panel layouts.

## Change Gate

- [x] Refresh Graphify main and audit graphs with `python tools/graphify_auto_refresh.py --once`.
- [x] Re-run UI orchestration contract tests.
- [x] Review main.cpp / AppState / UI helper `UNKNOWN_BLOCKING` entries.
- [x] Update this contract if selection routing, shortcut semantics, sidebar order, or result action visibility changes.

## Decision Log

| Date | Decision | Reason | Decider |
| --- | --- | --- | --- |
| 2026-06-04 | Added a dedicated UI orchestration contract instead of treating main.cpp regressions as an informal side effect of other modules. | Review found cross-panel UI drift had no direct contract owner even though main.cpp is the highest fan-out runtime surface. | Engineering |
| 2026-06-04 | Scoped this contract to routing/selection/workspace/menu semantics, not to every visual component. | The goal is to protect orchestration behavior without turning the contract into a duplicate of every module-specific UI document. | Engineering |

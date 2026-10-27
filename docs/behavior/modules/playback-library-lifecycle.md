# Playback / Library Lifecycle Behavior Contract

## Header

**ID:** BDD-PLAYBACK-LIBRARY
**Module:** Playback / Library Lifecycle
**Status:** Active
**Priority:** P0
**Owner:** Engineering
**Created:** 2026-06-03
**Last Updated:** 2026-07-11

## Product Behavior

**User promise:** Auditioning files and switching libraries never leaves stale audio, stale visual state, or stale DB state pretending to be current.
**Primary workflow:** User plays results, navigates playback history, switches DBs, and expects the current library boundary to be respected.
**Non-goals:** Empty-path playback requests are not the authority for DB-boundary cleanup.

## 中文语义

**用户承诺：** 用户听到的音频、看到的文件信息、封面、波形和结果列表必须属于当前库和当前文件，不会残留上一个库的状态。
**核心流程：** 用户播放搜索结果、切换文件、切换数据库，再切回原数据库；Aris 必须清掉跨库旧状态，并让新库的新选择成为唯一可见/可听状态。
**非目标：** 空路径的播放请求不是数据库边界清理的权威入口；库切换必须走显式边界清理语义。

## BDD Scenarios

### Happy Path

- Given 已满足以下前置条件：a user selects or random-plays a result
- When 执行以下操作：Aris calls playback load
- Then 应得到以下结果：the loaded audio, current file state, waveform, and info panel converge on the selected file

### Edge Cases

- Given 已满足以下前置条件：the user rapidly switches files, results, or DBs
- When 执行以下操作：async playback and UI state updates race
- Then 应得到以下结果：the user sees new file information, hears new file audio, and does not see old-library cover, waveform, or metadata residue
- Given 已满足以下前置条件：the user is playing a file, switches to another database, and later switches back
- When 执行以下操作：Aris restores the original library context
- Then 应得到以下结果：playback remains stopped or reloads only from an explicit current selection in that library; it never resumes stale audio by path alone

### Failure / Recovery

- Given 已满足以下前置条件：a target DB is corrupted or unavailable
- When 执行以下操作：the user switches databases
- Then 应得到以下结果：Aris recovers to a usable DB or a safe empty state without leaving stale audio or old-library visual state

### Non-Functional

- Given 已满足以下前置条件：playback is active
- When 执行以下操作：background scan/waveform/cover work is pending
- Then 应得到以下结果：playback load and control commands preempt background scan/waveform/cover work within one scheduler cycle.

## Interface Binding

| Interface / Entry | Kind | Source | Expected behavior |
| --- | --- | --- | --- |
| `AudioEngine::loadFileAsync()` | worker | Graphify | Starts latest playback load without stale overwrite |
| `SwitchDatabase` paths | API | semantic card | Own DB-boundary state reset and recovery |
| `SwitchDatabase` -> `discardLoadedFileForDatabaseBoundary()` -> `ResetDatabaseState` | API | Graphify + source scan | DB switch clears loaded-file audio authority before library state reset, so stale playback cannot masquerade as current library state |
| `discardLoadedFileForDatabaseBoundary()` | API | Graphify + source scan | Real AudioEngine API that clears loaded-file authority for DB switch |
| keyboard playback commands | UI | Graphify | Map to same playback contract |
| `Audio.loadFile` / switch diagnostics | diagnostics | QA protocol | Runtime evidence for load/switch |

## Graphify Binding

**Source graph:** `graphify-out/main/interface-call-graph.json` and `graphify-out/main/audit-call-graph.json`
**Impact target(s):** `loadFileAsync`, `SwitchDatabase`, `ResetDatabaseState`
**Audit classification requirement:** playback and DB-boundary unknowns must be reviewed because stale state is user-visible and high risk.
**Dynamic calls:** review UI command and async worker paths in `DYNAMIC_CALLS.md`.
**Unknown calls:** review `AudioEngine.cpp` and `main.cpp` playback unknowns.

## TDD / QA Proof

| Proof ID | Kind | Artifact / command / event | Pass signal | Status |
| --- | --- | --- | --- | --- |
| TEST-SWITCH-DB-BOUNDARY | integration | `TestSwitchDatabaseBoundary` | DB switch boundary semantics pass | active |
| TEST-STARTUP-RECONCILE | integration | `TestStartupReconciliation` | startup/library recovery passes | active |
| TEST-OGG-PLAYBACK | integration | `TestOggPlaybackSupport` | supported decode path passes | active |
| DIAG-AUDIO-LOAD | diagnostics | `Audio.loadFile`, switch stage diagnostics | events observed without stale error | active |

## Missing Coverage

- Missing proof: one end-to-end DB switch sameness test covering old audio, old waveform, old cover, old filters, and old results.
- Suspected stale proof: comments that imply empty `loadFileAsync("")` unloads audio; source scan confirms `discardLoadedFileForDatabaseBoundary()` is the real boundary API.
- Human-only verification: audible playback continuity and device behavior.

## Change Gate

- [x] Refresh Graphify main and audit graphs with `python tools/graphify_auto_refresh.py --once`.
- [x] Re-run switch/playback boundary tests.
- [x] Review playback/library `UNKNOWN_BLOCKING` entries.
- [x] Update this contract if playback latest-wins or library boundary semantics change.

## Decision Log

| Date | Decision | Reason | Decider |
| --- | --- | --- | --- |
| 2026-06-03 | Kept playback and library lifecycle in one contract for now, but added explicit switch-away/switch-back behavior and verified `discardLoadedFileForDatabaseBoundary()` as a real API. | The highest-risk behavior is the cross-boundary stale-state path, so the contract stays merged until standalone playback expansion justifies a split. | Engineering |
| 2026-06-03 | Future split plan: if playback grows, split into `playback-audition` and `library-lifecycle`, plus a cross-contract boundary clause for DB switch cleanup. | This preserves clear ownership while keeping the current stale-state boundary testable. | Engineering |

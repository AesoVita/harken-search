# Cover Replacement Browse Surface Behavior Contract

**ID:** BDD-COVER-REPLACEMENT-BROWSE-SURFACE
**Module:** Cover Replacement Browse Surface
**Status:** Active
**Priority:** P1
**Owner:** Engineering
**Created:** 2026-06-10
**Last Updated:** 2026-07-11

## Product Behavior

**User promise:** A sound designer can browse one Aris-style replacement cover wall, narrow it by folder tree / keyword / path / cover, resize tiles, and keep current preview identity aligned with the selected wall item.
**Primary workflow:** Open a library, enter the replacement browse surface, change scope through folder tree / keyword / path / cover controls, resize tiles, read top/bottom info, and inspect the current item without losing wall stability.
**Non-goals:** Feature-flag rollout, legacy cover-path retirement, playback or waveform redesign, metadata editor redesign, and startup/login flow changes.

## 中文语义

**用户承诺：** 音效师在新的封面替换浏览界面里，可以像当前 Aris 一样浏览封面墙，用 folder tree、关键词、路径和同封面过滤缩小范围；调整封面大小时不会丢失当前范围，当前预览也要与点击的封面保持同一套身份语义。
**核心流程：** 打开当前库，进入 replacement browse surface，通过 folder tree / keyword / path / cover 控制器切换范围，用滑块改变封面大小，同时保持 wall snapshot、top/bottom info 和当前预览都与当前活动范围一致。
**非目标：** 不做 feature flag rollout、legacy path retirement、playback / waveform redesign、metadata editor redesign，也不在这一 slice 内改动启动或登录流程。
## BDD Scenarios

### Happy Path

- Given 已满足以下前置条件：Slice 2 shared cover truth is already stable
- When 执行以下操作：the user changes folder tree, keyword, path, or cover scope
- Then 应得到以下结果：the visible wall, counts, and current preview stay on the same active identity and scope.

- Given 已满足以下前置条件：the browse surface is already showing a wall
- When 执行以下操作：the user changes tile size
- Then 应得到以下结果：the wall relayouts without resetting the active scope or triggering per-frame DB rebuild work.

### Edge Cases

- Given 已满足以下前置条件：keyword, path, and cover filters are combined
- When 执行以下操作：the user clears only one filter lane
- Then 应得到以下结果：the remaining scope stays intact and the wall does not fall back to a contradictory empty state.

- Given 已满足以下前置条件：a temporary `?|folder` identity is still active
- When 执行以下操作：the user clicks a wall tile and inspects the current item
- Then 应得到以下结果：current preview follows the same temporary or canonical identity path instead of switching to unrelated legacy truth.

### Failure / Recovery

- Given 已满足以下前置条件：a new filtered wall is still loading
- When 执行以下操作：a follow-up page or facet refresh has not settled yet
- Then 应得到以下结果：the browse surface keeps the previous stable display snapshot instead of flashing through a weaker intermediate wall.

- Given 已满足以下前置条件：a worker result returns for a stale query generation
- When 执行以下操作：the active browse scope has already changed
- Then 应得到以下结果：the stale result is discarded and does not overwrite the current wall or preview state.

### Non-Functional

- Given 已满足以下前置条件：a large library and active wheel / slider interaction
- When 执行以下操作：the replacement browse surface renders the visible wall
- Then 应得到以下结果：rendering stays bounded by visible items and does not perform synchronous DB truth rebuilding on each frame.

## Interface Binding

| Interface / Entry | Kind | Source | Expected behavior |
| --- | --- | --- | --- |
| `RenderSidebarPanelContent` | UI | Graphify / rg / product | Folder tree, keyword, path, and cover controls emit one active browse scope instead of independent wall truth paths. |
| `UpdateFacetCachesForFilter` | worker | Graphify / source / product | Filter changes request Slice 2 facet truth without bespoke UI-side regrouping. |
| `BuildCoverWallPageLoadResult` | API | Graphify / source / performance strategy | The first visible wall band is built from replacement outputs and can preserve the previous stable snapshot while follow-up work settles. |
| `CoverWallState` | API | Graphify / source | Owns the active replacement browse wall state rather than leaking state across unrelated UI surfaces. |
| `displaySnapshot` | API | Graphify / source | Holds the stable visual wall during staged refresh and prevents contradictory empty transients. |
| `pinnedCoverFilterItem` | API | Graphify / source / history | Stores the active cover-filter identity within the same scope used by wall selection and preview parity. |
| `CoverModule::GetCoverTextureByKey` | API | Graphify / source / product | Current preview and wall tiles load the same selected identity path without inventing a second truth lane. |
| `Database::getGlobalCoverWallEntriesPage` | DB | Graphify / source | No-filter browse pages come from the inherited Slice 2 aggregation owner. |
| `Database::getCoverFacets` | DB | Graphify / source | Keyword / path / tree scopes reuse the same inherited Slice 2 facet truth. |
| `Database::getFileIdsForCoverTileMembership` | DB | Graphify / source | Playlist artwork-drop keeps replacement tile membership explicit while Slice 5 continues retiring DB cover-key field debt. |

## Graphify Binding

**Source graph:** `graphify-out/main/interface-call-graph.json` and `graphify-out/main/audit-call-graph.json`
**Impact target(s):** `RenderSidebarPanelContent`, `UpdateFacetCachesForFilter`, `BuildCoverWallPageLoadResult`, `CoverWallState`, `displaySnapshot`, `pinnedCoverFilterItem`, `CoverModule::GetCoverTextureByKey`, `Database::getGlobalCoverWallEntriesPage`, `Database::getCoverFacets`, `Database::getFileIdsForCoverTileMembership`
**Audit classification requirement:** `UNKNOWN_BLOCKING` entries touching this module must be reviewed before completion.
**Dynamic calls:** review `graphify-out/main/DYNAMIC_CALLS.md` for this module.
**Unknown calls:** review `graphify-out/main/UNKNOWN_CALLS.md` for this module.
**2026-06-10 review note:** module-relevant `UNKNOWN_BLOCKING` rows are limited to inline/template helper symbols in `src/CoverBrowseSurfaceState.h`, local helper/lambda calls inside `RenderSidebarPanelContent`, and standard CRT/UI helper calls like `strncpy_s`; they are Graphify parser limits rather than hidden cross-module ownership for this slice.

## TDD / QA Proof

| Proof ID | Kind | Artifact / command / event | Pass signal | Status |
| --- | --- | --- | --- | --- |
| `TEST-COVER-BROWSE-SURFACE-BASELINE` | contract | `TestCoverBrowseSurfaceState`, `TestDatabaseFeatures`, `TestCoverClickPerf`, `TestBackgroundIoPolicy` | shared preset/info helper plus inherited Slice 2 truth stay stable while Slice 3 wiring begins; any remaining `TestBackgroundIoPolicy` failures must stay confined to the unrelated `Module dock` cluster | pass with unrelated blocker |
| `PROOF-COVER-TREE-SCOPE` | diagnostics / manual | `py scripts/qa_probe_cover_browse_surface.py --db-path .openclaw/fixtures/fixture-small/recat_wwise_fixture_small.db --auth-mode harness_multi --search-query WHOOSH --report-json .openclaw/reports/qa_probe_cover_browse_surface/slice3_targeted/report.json` + manual smoke | folder-tree/path scope returns the same wall/count truth while keyword scope remains active on the replacement surface | pass (`.openclaw/reports/qa_probe_cover_browse_surface/slice3_targeted/report.json`) |
| `PROOF-COVER-FILTER` | diagnostics / manual | same targeted browse-surface probe + manual smoke | specific cover filtering stays on the same pinned browse-surface identity contract as wall selection and preview state | pass (`.openclaw/reports/qa_probe_cover_browse_surface/slice3_targeted/report.json`) |
| `PROOF-COVER-SLIDER-STABILITY` | diagnostics / manual | same targeted browse-surface probe + manual smoke | tile-size changes relayout the wall without per-frame DB work or scope reset | pass (`.openclaw/reports/qa_probe_cover_browse_surface/slice3_targeted/report.json`) |
| `PROOF-COVER-PREVIEW-PARITY` | targeted / manual | same targeted browse-surface probe + manual smoke | wall selection, cover-filter result, and current preview/current item resolve the same identity | pass (`.openclaw/reports/qa_probe_cover_browse_surface/slice3_targeted/report.json`) |

## Missing Coverage

- Missing proof: none for Slice 3 keyword/tree/path/cover/slider/preview targeted runtime evidence; the dedicated browse-surface probe now exists at `scripts/qa_probe_cover_browse_surface.py`.
- Suspected stale proof: none for Slice 3 targeted browse-surface proof; remaining risk belongs to later integrated-mode rollout and rollback, not this targeted replacement surface.
- Screenshot-level visual smoke: recorded by `.openclaw/reports/qa_probe_cover_browse_surface/slice3_targeted/window_before_browse_surface_probe.png` and `.openclaw/reports/qa_probe_cover_browse_surface/slice3_targeted/window_after_browse_surface_probe.png`, which show the Aris-style browse layout, readable top/bottom info, keyword scope, and pinned cover-filter state.
- Human-only verification: real mouse-wheel/slider feel on a large live library remains a later integrated-mode QA item, not a blocker for the targeted Slice 3 browse-surface contract.

## Change Gate

- [x] Refresh Graphify main and audit graphs with `python tools/graphify_auto_refresh.py --once`.
- [x] Re-run this module's TDD / QA proof.
- [x] Review module-relevant `UNKNOWN_BLOCKING` entries.
- [x] Update this contract if product behavior or interface binding changed.

## Decision Log

| Date | Decision | Reason | Decider |
| --- | --- | --- | --- |
| 2026-06-10 | Slice 3 must use one browse-surface owner on top of Slice 2 truth instead of rebuilding wall truth in render. | The replacement surface only reduces risk if visible controls and preview parity stop inventing their own truth lanes. | Product / Engineering |
| 2026-06-10 | `CoverWallLab` may donate reusable modules, but it cannot become the permanent production owner. | The user asked for a replacement of Aris's current cover feature, not a second product path. | Product / Engineering |
| 2026-06-10 | `AppState::CoverWallState` stays the production browse-surface owner, while `aura::coverwall::BrowseSurfaceInfo` holds shared preset/info semantics reused by both lab and production. | The production wall already owns snapshot/paging/filter identity state, so the safest extraction is a helper-level cut rather than a second state container. | Product / Engineering |
| 2026-06-10 | Folder-tree/path scope must stop trimming the cover wall in render with `folderSearchBuffer` and instead rely on the shared Slice 2 truth path. | A second UI-side trim would let the visible wall disagree with the result/count truth that Slice 2 already stabilized. | Product / Engineering |
| 2026-06-10 | Async refresh must restore browse-surface preview selection inside the new filtered result set before the legacy current-item fallback can take over. | Replacement browse parity is broken if wall/filter scope changes clear selection and the metadata panel silently drifts back to an unrelated current file. | Product / Engineering |

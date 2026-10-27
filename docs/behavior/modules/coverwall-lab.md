# CoverWallLab Behavior Contract

**ID:** BDD-COVERWALL-LAB
**Module:** CoverWallLab
**Status:** Active
**Priority:** P1
**Owner:** Engineering
**Created:** 2026-06-10
**Last Updated:** 2026-07-11

## Product Behavior

**User promise:** A sound designer can open a focused cover-wall lab, browse a visual wall, filter by path, keyword, or cover, and resize tiles without losing context.
**Primary workflow:** Open the lab, import or use fixture data, browse covers, narrow by path, narrow by keyword, click a cover, adjust tile size, and keep result/count truth stable.
**Non-goals:** Playback, waveform, playlist, metadata editing, production library mutation, and replacement of the main Aris UI.

## 中文语义

**用户承诺：** 音效师可以打开一个独立封面墙实验软件，用最少干扰完成入库浏览、路径搜索、关键词搜索、封面搜索和封面尺寸调节。
**核心流程：** 打开实验软件，导入文件夹或使用内置示例数据，按路径/关键词/封面过滤结果，用尺寸滑条或预设切换封面大小，并保持当前筛选状态不被重置。
**非目标：** 不做播放、波形、播放列表、元数据编辑、生产库迁移，也不替代主 Aris 界面。

## BDD Scenarios

### Happy Path

- Given 已满足以下前置条件：the lab has a dataset
- When 执行以下操作：the user types a keyword
- Then 应得到以下结果：the wall and result count update for that keyword.

- Given 已满足以下前置条件：keyword and path filters are active
- When 执行以下操作：the user clicks a cover tile
- Then 应得到以下结果：the cover filter applies within the existing keyword and path scope.

### Edge Cases

- Given 已满足以下前置条件：any filters are active
- When 执行以下操作：the user changes tile size with the slider or presets
- Then 应得到以下结果：keyword, path, cover selection, and count truth remain stable.

- Given 已满足以下前置条件：the available wall width is very small
- When 执行以下操作：columns are recalculated
- Then 应得到以下结果：the wall still keeps at least one valid column.

### Failure / Recovery

- Given 已满足以下前置条件：an imported folder has no supported audio files
- When 执行以下操作：import is requested
- Then 应得到以下结果：the lab keeps the previous stable dataset and reports a clear status.

### Non-Functional

- Given 已满足以下前置条件：the user changes tile size repeatedly
- When 执行以下操作：the UI relayouts the wall
- Then 应得到以下结果：no synchronous database work is required for each slider frame.

## Interface Binding

| Interface / Entry | Kind | Source | Expected behavior |
| --- | --- | --- | --- |
| `CoverWallLab` target | UI | Product / CMake | Builds as an independent executable. |
| `CoverWallLabState` | API | Product / source | Owns lab-local filters, results, facets, and tile size. |
| `importFolder` | worker | Product | Imports supported files into lab memory without mutating source audio. |
| `setKeyword` | API | Product | Updates result wall and count truth. |
| `selectPath` | API | Product | Filters results by folder/path. |
| `selectCover` | API | Product / Graphify | Filters results by selected cover key. |
| `setTileSizePx` | API | Product | Clamps size and preserves filters. |
| `saveSettings` / `loadSettings` | API | Product | Persists lab-local tile size. |

## Graphify Binding

**Source graph:** `graphify-out/main/interface-call-graph.json` and `graphify-out/main/audit-call-graph.json`
**Impact target(s):** `CoverWallLabState`, `CoverWallLab`, future `Database::searchFiles`, future `Database::getGlobalCoverWallEntriesPage`, future `CoverModule`.
**Audit classification requirement:** `UNKNOWN_BLOCKING` entries touching this module must be reviewed before completion.
**Dynamic calls:** review `graphify-out/main/DYNAMIC_CALLS.md` for this module.
**Unknown calls:** review `graphify-out/main/UNKNOWN_CALLS.md` for this module.

## TDD / QA Proof

| Proof ID | Kind | Artifact / command / event | Pass signal | Status |
| --- | --- | --- | --- | --- |
| TEST-COVERWALL-LAB-SIZE-CONTRACT | contract | `TestCoverWallLabContract` | pass | active |
| TEST-COVERWALL-LAB-FILTER-STATE | contract | `TestCoverWallLabContract` | pass | active |
| TEST-COVERWALL-LAB-COUNT-TRUTH | contract | `TestCoverWallLabContract` | pass | active |
| QA-COVERWALL-LAB-GUI | manual optional | launch `CoverWallLab` | final visual quality review is optional for the Stage 5 automated baseline | optional-not-run |

## Missing Coverage

- Missing proof: automated screenshot or GUI-driver coverage for the real wall.
- Suspected stale proof: production DB-backed search integration is not part of the first slice.
- Human-only verification: final visual quality and slider feel.

## Change Gate

- [x] Refresh Graphify main and audit graphs with `python tools/graphify_auto_refresh.py --once`.
- [x] Re-run this module's TDD / QA proof.
- [x] Review module-relevant `UNKNOWN_BLOCKING` entries.
- [x] Update this contract if product behavior or interface binding changed.

## Decision Log

| Date | Decision | Reason | Decider |
| --- | --- | --- | --- |
| 2026-06-10 | Build a separate lab before simplifying main UI. | Isolates the cover-wall workflow and avoids more `main.cpp` coupling. | Product / Engineering |

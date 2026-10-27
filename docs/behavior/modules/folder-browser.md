# Folder Browser Behavior Contract

## Header

**ID:** BDD-FOLDER-BROWSER
**Module:** Folder Browser
**Status:** Active
**Priority:** P1
**Owner:** Engineering
**Created:** 2026-07-06
**Last Updated:** 2026-07-11

## Product Behavior

**User promise:** Aris can browse normal filesystem folders, audition audio immediately from path, and expose explicit next actions before any database dependency is required.
**Primary workflow:** User opens Browser mode, navigates the system folder tree or path bar, clicks a disk audio file, hears it immediately, inspects waveform, then optionally bookmarks, scans, adds the folder to library, reveals it in Explorer, or sends/drags the file to a DAW.
**Non-goals:** This contract does not promise a full Resonic-class polish pass, automatic recursive indexing on navigation, or live DAW success without the target host/runtime already being available.

## 中文语义

**用户承诺：** 用户在 Aris 的 Browser 里浏览硬盘文件夹时，不需要先入库也能直接点到哪听到哪，并且能对当前路径执行收藏、扫描、加入库、资源管理器打开、发送到宿主等显式动作。
**核心流程：** 用户进入 Browser → 用左侧系统文件夹树 / 顶部路径栏进入目录 → 中间列表点击音频文件 → Aris 立刻按路径加载试听并联动主波形 → 用户按需执行 Bookmark Folder / Scan This Folder / Add Folder to Library / Reveal in Explorer / Send to DAW / Drag to DAW。
**非目标：** 本契约不要求完整复刻 Resonic，不要求浏览时自动入库，也不保证在未满足宿主前置条件时仍能声称发送成功。

## BDD Scenarios

### Happy Path

- Given 已满足以下前置条件：a filesystem audio file has no database row
- When 执行以下操作：the user selects it in Browser
- Then 应得到以下结果：Aris auditions it by path, sets truthful no-DB identity, and schedules the same path for main waveform detail
- Given 已满足以下前置条件：the user wants to browse disks directly
- When 执行以下操作：the Browser sidebar is open
- Then 应得到以下结果：a dedicated system folder tree and current-folder actions are available without reusing the database folder filter as the primary browser surface
- Given 已满足以下前置条件：the user remembers only part of a folder name
- When 执行以下操作：the user types into the Browser folder-search box
- Then 应得到以下结果：Aris searches system folders asynchronously, matches folder names, full paths, and matching file names by returning their parent folders, keeps descending after direct hits so broad terms show all matching folders, filters the lower system folder tree with folder nodes only, expands matching chains once, scrolls to the first direct hit, and keeps the lower tree limited to hit chains while opened/clicked node children are shown in the Related Paths pane without adding audio/file leaves
- Given 已满足以下前置条件：the user clicks a system folder path in the Browser tree
- When 执行以下操作：the user searches folders afterward
- Then 应得到以下结果：Aris searches inside that clicked normal-tree path as the current search scope instead of scanning all drives, but clicking a filtered search result only navigates the center Browser list and keeps the existing search results visible
- Given 已满足以下前置条件：the user clicked a normal system-tree folder to set the Browser folder-search scope
- When 执行以下操作：the Browser sidebar redraws or the user clicks a filtered search result
- Then 应得到以下结果：Aris keeps the clicked normal-tree path as the search scope until refresh or another normal-tree click, without an extra System Folder Tree title; Related Paths reflects its own selected/opened folder and does not represent the search scope. Commit 2e9692fb removed the extra title as an intentional Filter layout decision.
- Given 已满足以下前置条件：the Browser system folder tree has expanded nodes or an active folder-search scope
- When 执行以下操作：the user presses the refresh button beside the Browser folder-search field
- Then 应得到以下结果：Aris behaves like the database folder tree refresh: it clears folder-search text, clears the clicked search scope, cancels stale search results, clears the tree expansion storage, and reloads the tree without locating the current Browser path
- Given 已满足以下前置条件：the Browser system folder tree is filtered
- When 执行以下操作：the user clicks a folder node
- Then 应得到以下结果：Aris updates the center Browser list to that folder and does not audition audio or rebuild the tree around the newly selected path
- Given 已满足以下前置条件：the user switches Browser scope to recursive child-audio mode
- When 执行以下操作：the user switches tabs or relaunches Aris
- Then 应得到以下结果：the Browser scope restores truthfully instead of silently falling back to current-directory mode

### Edge Cases

- Given 已满足以下前置条件：the Browser selection is multi-file
- When 执行以下操作：the user chooses Send to DAW from the Browser row context
- Then 应得到以下结果：the payload is built from the Browser-selected paths, not stale database/search selection
- Given 已满足以下前置条件：the Browser entry is a directory
- When 执行以下操作：the user opens the context menu
- Then 应得到以下结果：folder-scoped actions stay available even though playback-only actions do not

### Failure / Recovery

- Given 已满足以下前置条件：a folder cannot be enumerated because of permissions or filesystem errors
- When 执行以下操作：the Browser system tree expands that node
- Then 应得到以下结果：Aris keeps the UI responsive and fails locally to that node instead of blocking Browser mode
- Given 已满足以下前置条件：a DAW send path is unavailable
- When 执行以下操作：Browser send is requested
- Then 应得到以下结果：Aris reports truthful failure state and does not silently fall back to an unrelated database selection

### Non-Functional

- Given 已满足以下前置条件：a large folder or drive tree
- When 执行以下操作：Browser mode renders the list and system tree
- Then 应得到以下结果：row materialization stays clipped, folder-tree expansion stays lazy, and no recursive scan starts until the user explicitly asks for it
- Given 已满足以下前置条件：a broad folder-search term returns too many filesystem folders
- When 执行以下操作：Browser renders the filtered system tree
- Then 应得到以下结果：Aris keeps the UI responsive, continues searching in the background until all roots finish or the query is cancelled, progressively displays every matching folder, and shows a persistent loading/result-count indicator while the result set is still incomplete

## Interface Binding

| Interface / Entry | Kind | Source | Expected behavior |
| --- | --- | --- | --- |
| `BeginBrowserPathAudition()` | API | source | Browser path audition centralizes `loadFileAsync`, no-DB identity, and detail-wave scheduling |
| `BuildBrowserTransferSourcePaths()` | API | source | Browser send derives file paths from Browser selection semantics |
| `SendToDAWForPaths()` | API | source | Browser context send uses Browser-selected paths instead of DB-result selection |
| `RenderBrowserSystemFolderTree()` | UI | source | Browser sidebar exposes a filesystem tree distinct from DB folder filters |
| Browser folder search + system tree | UI | source | Browser sidebar supports folder name/path/file-name search while rendering folders only, keeps descending after direct hits, filters/expands folder-hit chains only, preserves the existing result set when a search result is clicked, shows loading/result counts while incomplete, and uses the Related Paths pane for child folders of the selected/opened hit |
| Browser scope persistence | API | source | `CurrentDir` / `RecursiveAudio` survives tab sync and settings restore |
| current-folder action buttons | UI | source | Reveal / Bookmark / Scan / Add Folder actions are explicit in Browser mode |
| Browser row context menu | UI | source | File rows and directory rows expose truthful action subsets |

## Graphify Binding

**Source graph:** `graphify-out/main/interface-call-graph.json` and `graphify-out/main/audit-call-graph.json`
**Impact target(s):** `OpenFileInFileBrowser`, Browser row-selection block in `RenderUI`, `ScheduleDeferredWaveformDetail`, `AudioEngine::loadFileAsync`, `SendToDAW`, `BuildTransferPayloadForPaths`
**Audit classification requirement:** Browser selection, send, and waveform handoff must stay attributable because `main.cpp` routing drift can silently route actions to stale DB state.
**Unknown calls:** Review Browser-related `UNKNOWN_BLOCKING` nodes if send/playback/waveform ownership becomes ambiguous.

## TDD / QA Proof

| Proof ID | Kind | Artifact / command / event | Pass signal | Status |
| --- | --- | --- | --- | --- |
| TEST-BROWSER-SOURCE-CONTRACT | source contract | `TestBrowserSearch` | Browser source contract covers no-DB audition, Browser send-path truth, and system-tree/current-folder action surfaces | active |
| TEST-DAW-SEND-CONTRACT | source/supporting | Browser source-contract assertions in `TestBrowserSearch` | Browser row send remains bound to Browser-selected paths instead of stale DB selection | active |
| TEST-OPEN-WITH-CONTRACT | source contract | `TestOpenWith` | Open-with / Browser-entry routing still reaches Browser playback and Explorer reveal paths | active |
| MANUAL-BROWSER-GUI | manual | Browser mode human probe | system tree feel, click-to-hear, waveform convergence, and folder actions remain coherent | human-required |

## Missing Coverage

- Missing proof: live GUI feel for system-tree expansion and top-waveform convergence is still human-only.
- Missing proof: real Pro Tools / DAW import from Browser selection remains environment-dependent.
- Residual gap: folder-tree node enumeration limits are source-reviewed but not yet benchmarked on every pathological network/permission case.

## Change Gate

- [x] Refresh Graphify main and audit graphs with `py tools/graphify_auto_refresh.py --once`.
- [x] Re-run `TestBrowserSearch`, `TestSendToTargetSurfaceModel`, and `TestOpenWith`.
- [x] Update this contract if Browser path identity, folder actions, or Browser-side send semantics change.

## Decision Log

| Date | Decision | Reason | Decider |
| --- | --- | --- | --- |
| 2026-07-06 | Centralize Browser audition into `BeginBrowserPathAudition()`. | Prevent Browser click, deferred-select, open-with, and mini-wave seek from drifting into mixed DB/path identity semantics. | Engineering |
| 2026-07-06 | Add Browser-side path send helper instead of reusing DB-result selection. | Browser row send must be truthful for unindexed files and multi-select. | Engineering |
| 2026-07-06 | Ship a lazy system folder tree in Browser sidebar rather than repurposing the DB folder filter tree. | Disk browsing must be distinct from database filtering in the MVP promise. | Engineering |
| 2026-07-06 | Persist Browser recursive child-audio scope across tab sync and settings restore. | “含子音频目录” is workflow state, not a transient toggle. | Engineering |

# Cover Replacement Retirement Behavior Contract

**ID:** BDD-COVER-REPLACEMENT-RETIREMENT
**Module:** Cover Replacement Retirement
**Status:** Active
**Priority:** P1
**Owner:** Engineering
**Created:** 2026-06-10
**Last Updated:** 2026-07-11

## Product Behavior

**User promise:** once replacement cover mode has already proven parity and rollback, the final production cover wall retires unstable legacy truth lanes without breaking preview parity, rebuild, or rollback.
**Primary workflow:** launch Aris, browse/filter the cover wall, inspect current preview/current item, and recover through rebuild or rollback without relying on hidden old-path truth for supported-library states.
**Non-goals:** removing all compatibility fields in one patch, changing Slice 1 storage mode, or deleting rollback scaffolding before the proof matrix is complete.

## 中文语义

**用户承诺：** 在 replacement cover 模式已经证明集成态可用之后，最终生产封面墙要把不稳定的旧 truth lane 退役掉，但不能牺牲 preview/current item 一致性、rebuild 可恢复性或 rollback 安全性。
**核心流程：** 启动 Aris，进入生产封面墙，执行 no-filter / keyword / path / cover browse，检查当前预览与当前项语义；如果做了 retirement 删减，仍然可以通过 rebuild 或 rollback 恢复稳定可用的墙面，而不是依赖隐藏旧链路兜底。
**非目标：** 不在一次补丁里删光所有兼容字段，不推翻 Slice 1 的 `DB-first dual storage`，也不在 proof 未完成前移除 rollback scaffolding。
## BDD Scenarios

### Happy Path

- Given 已满足以下前置条件：Slice 4 integrated mode is already green in `harness_multi`
- When 执行以下操作：the production cover wall runs on supported-library states
- Then 应得到以下结果：no-filter / keyword / path / cover workflows use one replacement truth owner instead of a hidden legacy display-truth bailout.

- Given 已满足以下前置条件：the user changes scope and inspects the current item
- When 执行以下操作：preview/current item parity is evaluated after retirement cuts
- Then 应得到以下结果：the preview remains aligned with the same replacement identity contract.

### Edge Cases

- Given 已满足以下前置条件：a preliminary ?|... identity is still valid because canonical truth is incomplete
- When 执行以下操作：retirement narrows old supported-state fallback lanes
- Then 应得到以下结果：preliminary identity behavior remains explicit instead of disappearing or silently reviving the old unstable wall path.

- Given 已满足以下前置条件：rollback mode is triggered after retirement work
- When 执行以下操作：integrated mode is turned off or compatibility baseline is exercised
- Then 应得到以下结果：the app still reaches a usable old-path-compatible wall without re-ingesting source audio.

### Failure / Recovery

- Given 已满足以下前置条件：a retirement cut removes or narrows one old truth lane
- When 执行以下操作：rebuild / repair is invoked
- Then 应得到以下结果：wall truth can be regenerated or repaired without reviving undocumented dual-truth behavior.

- Given 已满足以下前置条件：a supported-library scenario still secretly needs a legacy `cover_key` path
- When 执行以下操作：the delete gate audit runs
- Then 应得到以下结果：that dependency is recorded as retained debt instead of being guessed away.

### Non-Functional

- Given 已满足以下前置条件：retirement touches global wall, filtered facets, and preview restore
- When 执行以下操作：the production wall is exercised under prime/repeat browse flows
- Then 应得到以下结果：no-filter / keyword / path / cover interactions remain bounded and do not regress startup or interaction stability.

## Interface Binding

| Interface / Entry | Kind | Source | Expected behavior |
| --- | --- | --- | --- |
| `BuildCoverWallPageLoadResult` | API | Graphify / source | Supported-library global wall shaping no longer relies on unstable legacy display truth paths. |
| `CoverWallSource::SearchFallback` / `fallbackItems` | UI | source / history | Search fallback remains only where product semantics still justify it, not as the normal supported-state truth owner. |
| `displaySnapshot` | API | Graphify / source | Stable wall snapshot remains valid during retirement and does not mask contradictory old/new truth. |
| `ApplyAsyncSearchResult` | API | Graphify fallback / source | Preview restore keeps current item parity after old-path narrowing. |
| `Database::getGlobalCoverWallEntriesPage` | DB | Graphify / source | No-filter wall continues to page through the production truth owner safely after retirement cuts. |
| `Database::getCoverFacets` | DB | Graphify / source | Keyword/path/cover filtered walls remain deduped and replacement-owned. |
| `Database::getFileIdsForCoverTileMembership` | DB | Graphify / source | Playlist artwork-drop resolves the current replacement cover tile through registry/phash-equivalent keys, reads the canonical `cover_tile_membership` index, and self-heals missing canonical rows on writable legacy DBs; temporary folder identity remains explicit. |
| `embedded_cover_key` / `folder_cover_key` / `cover_cache_registry` | DB | design / source | May remain as explicit bridge or rollback scaffold, but not as an undocumented normal-path dependency for supported-library states. |

## Graphify Binding

**Source graph:** `graphify-out/main/interface-call-graph.json` and `graphify-out/main/audit-call-graph.json`
**Impact target(s):** `BuildCoverWallPageLoadResult`, `ApplyAsyncSearchResult`, `RenderSidebarPanelContent`, `CoverModule::AsyncLoaderThread`, `Database::getGlobalCoverWallEntriesPage`, `Database::getCoverFacets`, `Database::getFileIdsForCoverTileMembership`
**Graphify no-match note:** `RestoreBrowseSurfacePreviewAfterRefresh` is a local helper and not indexed as a standalone node; use `ApplyAsyncSearchResult` plus `rg/source scan` as the required fallback evidence.
**Audit classification requirement:** `UNKNOWN_BLOCKING` entries touching this module must be reviewed before completion.
**Dynamic calls:** review `graphify-out/main/DYNAMIC_CALLS.md` for this module.
**Unknown calls:** review `graphify-out/main/UNKNOWN_CALLS.md` for this module.

## TDD / QA Proof

| Proof ID | Kind | Artifact / command / event | Pass signal | Status |
| --- | --- | --- | --- | --- |
| `TEST-COVER-RETIREMENT-SOURCE-CONTRACT` | contract | `TestBackgroundIoPolicy` / `TestCoverReplacementIntegration` retirement source assertions | old supported-state fallback lanes are narrowed only where delete gate allows, and production artwork drag/drop uses replacement tile membership instead of old bridge names | passed / extended 2026-06-11 |
| `TEST-COVER-RETIREMENT-DB-COMPAT` | contract | `TestDatabaseFeatures` focused `cover_tile_membership` assertions | ?\|... replacement tile membership resolves matching file ids; registry/phash-equivalent keys resolve one tile to the equivalent file-id group; canonical `cover_tile_membership` can return ids when legacy audio cover-key fields are empty, can self-heal rows for writable legacy DBs, and no longer revives legacy audio cover-key fields when canonical membership rows are absent | passed / legacy field fallback retired |
| `TEST-COVER-RETIREMENT-MEMBERSHIP-MAINTENANCE` | contract | `TestDatabaseFeatures` focused maintenance assertions + `TestBackgroundIoPolicy` source contract | legacy/open rearm marks `cover_tile_membership_needs_maintenance`, incremental repair batches repopulate canonical membership rows, and the background worker keeps an explicit repair lane for pre-index libraries | passed |
| `PROOF-COVER-RETIRE-NOFILTER-PRIME-REPEAT` | runtime | `.openclaw/reports/qa_probe_cover_replacement_retirement/harness_multi/report.json` | no-filter prime/repeat remains stable after retirement cut | passed |
| `PROOF-COVER-RETIRE-KEYWORD-PRIME-REPEAT` | runtime | `.openclaw/reports/qa_probe_cover_replacement_retirement/harness_multi/report.json` | keyword prime/repeat remains stable after retirement cut | passed |
| `PROOF-COVER-RETIRE-PATH-PRIME-REPEAT` | runtime | `.openclaw/reports/qa_probe_cover_replacement_retirement/harness_multi/report.json` | path prime/repeat remains stable after retirement cut | passed |
| `PROOF-COVER-RETIRE-FILTER` | runtime | `.openclaw/reports/qa_probe_cover_replacement_retirement/harness_multi/report.json` | specific-cover filter parity remains stable after retirement cut | passed |
| `PROOF-COVER-RETIRE-PREVIEW-PARITY` | runtime | `.openclaw/reports/qa_probe_cover_replacement_retirement/harness_multi/report.json` | preview/current item parity survives retirement | passed |
| `PROOF-COVER-RETIRE-ROLLBACK` | runtime | `.openclaw/reports/qa_probe_cover_replacement_retirement/harness_multi/report.json` | mode-off or compatibility rollback remains usable | passed |
| `PROOF-COVER-RETIRE-REBUILD` | runtime / repair | `.openclaw/reports/qa_probe_cover_rebuild_8082/harness_multi/report.json` plus `.openclaw/reports/qa_probe_cover_replacement_retirement/harness_multi/report.json` | real `8082` package import rebuilt cover truth and the combined retirement package binds that proof with all eight proof keys green | passed |
| `PROOF-COVER-RETIRE-LEGACY-FALLBACK-AUDIT` | audit | Slice 5 card + source/DB audit + source-contract bridge naming | supported-library matrix proves `SearchFallback` is not the normal active runtime source; old playlist bridge names are retired, with DB cover-key fields retained as explicit debt | passed / DB field debt open |

## Missing Coverage

- Missing proof: final deletion/removal of all DB `cover_key` compatibility surfaces is not proven; current state has deleted the file-path reverse lookup bridge, replaced the playlist-only bridge name with replacement tile membership semantics, and fully retired the legacy audio-field fallback inside `getFileIdsForCoverTileMembership()`, but other DB compatibility surfaces such as `embedded_cover_key` / `folder_cover_key` storage columns still remain as data/schema debt.
- Suspected stale proof: old fixture-switch rebuild proof is flaky and has been replaced for closure by the standalone real-import `8082` rebuild report.
- Human-only verification: visual parity after future real deletion slices may still need screenshot or operator confirmation even when the probe is green.
- RESOLVED (`A-20260710-002`, 2026-07-11): the integrated-mode evidence helper was restored and `TestCoverReplacementIntegration` now executes successfully; Status is normalized to `Active`, and the proof-table `?\|...` token is escaped. Full evidence: `.local/stage-5-closure-logs/20260710/51-full-ctest-72-green-candidate.stdout.log` (72/72 PASS, 0 FAIL, 0 NOT_RUN).

## Verification Ledger

- 2026-06-11: `py tools\behavior_regression_suite.py --module cover-replacement-retirement --run` passed `TestCoverReplacementFoundation`, `TestCoverReplacementIntegration`, `TestDatabaseFeatures`, and `TestBackgroundIoPolicy`.
- 2026-06-11: `py scripts\qa_probe_cover_replacement_retirement.py --db-path .openclaw\fixtures\fixture-small\recat_wwise_fixture_small.db --auth-mode harness_multi --rebuild-proof-report .openclaw\reports\qa_probe_cover_rebuild_8082\harness_multi_20260611_042947_full_retire\report.json --report-json .openclaw\reports\qa_probe_cover_replacement_retirement\harness_multi_full_retire_after_apply_self_heal.json` passed all eight retirement proofs, including `PROOF-COVER-RETIRE-FILTER`.
- 2026-06-11: `cover_tile_membership` self-heal is now also applied at production specific-cover filter entry points (`WindowsMain.cpp` QA command + `main.cpp` cover-wall click path) so the read-only async worker still queries canonical membership after the primary writable DB repairs legacy libraries.
- 2026-06-11: `scripts\qa_probe_cover_wall_perf.py` now refuses `pending_count_follow_up=true` states when deciding count stability, preventing false retirement/filter green states during refresh.
- 2026-06-11: `cmake --build build_dbg --config Debug --target Aris -- /m:1` passed after splitting playlist artwork-drop lookup from the file-path bridge.
- 2026-06-11: `build_dbg\Debug\TestBackgroundIoPolicy.exe | Select-String "DB cover-key reverse lookup APIs are explicitly named|Remaining cover-key reverse lookups are split|Production UI no longer routes artwork drag/drop"` returned all three `PASS` lines.
- 2026-06-11: `build_dbg\Debug\TestDatabaseFeatures.exe | Select-String "temporary folder identity tile membership returns all matching file ids|temporary folder identity cover filter matches tile membership semantics"` returned both `PASS` lines.
- 2026-06-11: file-path reverse lookup bridge deletion removed the retired file-path bridge from production and test stubs; `CoverModule::AsyncLoaderThread` now relies on DB thumb cache and the representative file path instead of scanning same-key file paths.
- 2026-06-11: `py tools\graphify_debug_impact.py getFileIdsForCoverTileMembership --rebuild-if-stale` confirms playlist artwork-drop membership is isolated to quickbar/sidebar artwork-drop UI paths.
- 2026-06-11: Source-contract output confirmed `Legacy SearchFallback` is quarantined, old DB bridge names are retired, and production UI/cover decode paths no longer call the ambiguous legacy reverse-lookup name.
- 2026-06-11: `cmake --build build_dbg --config Debug --target Aris -- /m:1` passed after replacing the playlist compatibility bridge name with replacement tile membership.
- 2026-06-11: `build_dbg\Debug\TestBackgroundIoPolicy.exe | Select-String "DB cover-key compatibility bridge names are retired|Production artwork drag/drop uses replacement tile membership|Production code no longer routes cover work through the file-path compatibility bridge"` returned all three `PASS` lines.
- 2026-06-11: `py tools\behavior_regression_suite.py --module cover-replacement-retirement --run` passed `TestCoverReplacementFoundation`, `TestCoverReplacementIntegration`, `TestDatabaseFeatures`, and `TestBackgroundIoPolicy`.
- 2026-06-11: `py tools\graphify_debug_impact.py getFileIdsForCoverTileMembership --rebuild-if-stale` showed production callers only from `RefreshPlaylistTargetAfterDrop` and `RenderSidebarPanelContent`; `Database.cpp` self and `usesAdaptiveMiniWaveform()` are Graphify parser noise, not additional business routes.
- 2026-06-11: `TestDatabaseFeatures` now proves `getFileIdsForCoverTileMembership()` expands one registry/phash-equivalent key or normalized `PHASH|...` tile into the full equivalent file-id group.
- 2026-06-11: `TestBackgroundIoPolicy` now requires `getFileIdsForCoverTileMembership()` to call `ResolveEquivalentCoverKeys(this, tileIdentityKey)` and use batched `embedded_cover_key IN (...)` / `folder_cover_key IN (...)`, preventing regression to a single-key legacy lookup.
- 2026-06-11: `build_dbg\Debug\TestDatabaseFeatures.exe | Select-String "cover tile membership expands one key to equivalent registry group ids|normalized cover tile membership resolves equivalent registry group ids"` returned both `PASS` lines.
- 2026-06-11: `cmake --build build_dbg --config Debug --target Aris -- /m:1` passed after stopping the stale running `Aris.exe` that held the output file lock.
- 2026-06-11: `py tools\behavior_regression_suite.py --module cover-replacement-retirement --run` passed after equivalent-group membership backing changed; the suite refreshed Graphify and ran the four retirement tests with `0` failures.
- 2026-06-11: `py tools\graphify_debug_impact.py getFileIdsForCoverTileMembership --rebuild-if-stale` now shows `ResolveEquivalentCoverKeys()` as a direct callee, with production callers still limited to quickbar/sidebar artwork-drop paths.
- 2026-06-11: `cover_tile_membership` was added as the canonical file-id membership index for replacement cover tiles; `syncCoverRegistry()`, `rebuildCoverTileMembershipIndex()`, `finalizeCoverRegistryDeltas()`, and `upsertCoverRegistry()` now maintain it.
- 2026-06-11: `$env:AURA_TEST_DATABASE_FEATURES_CASE='cover_tile_membership'; build_dbg\Debug\TestDatabaseFeatures.exe` passed the temporary identity, equivalent registry/phash group, and canonical-index-before-legacy-field assertions.
- 2026-06-11: `TestBackgroundIoPolicy` source contract now requires the `cover_tile_membership` table/rebuild entry and requires `getFileIdsForCoverTileMembership()` to read that table before the explicit retained-debt fallback.
- 2026-06-11: `py scripts\qa_probe_cover_rebuild_8082.py --scan-root "H:\Sound effect\综合\8082"` wrote `.openclaw\reports\qa_probe_cover_rebuild_8082\harness_multi_20260611_035705\report.json`; the real import/rebuild passed with `3897` rows, `44` cover registry rows, `2172` `cover_tile_membership` rows, `44` membership keys, `display_count=44`, `visible_ready=28`, and `fallback_items=0`.
- 2026-06-11: `getFileIdsForCoverTileMembership()` now self-heals missing canonical membership rows on writable legacy DBs by syncing the requested equivalent keys into `cover_tile_membership` before using the retained legacy field fallback.
- 2026-06-11: `$env:AURA_TEST_DATABASE_FEATURES_CASE='cover_tile_membership'; build_dbg\Debug\TestDatabaseFeatures.exe` passed the new self-heal assertions: a legacy row still resolves and repopulates `cover_tile_membership` after the canonical rows are deleted.
- 2026-06-11: `TestBackgroundIoPolicy` now requires the self-heal step (`SyncCoverTileMembershipForKeys(m_db, equivalentKeys)`) to remain between canonical membership reads and the retained legacy field fallback.
- 2026-06-11: `$env:AURA_TEST_DATABASE_FEATURES_CASE='cover_tile_membership'; build_dbg\Debug\TestDatabaseFeatures.exe` also passed the new maintenance assertions: empty legacy membership tables report pending maintenance, `repairCoverTileMembershipBatch()` repopulates rows incrementally, clears the marker after drain, and reopen re-arms `cover_tile_membership_needs_maintenance` when canonical membership is empty.
- 2026-06-11: `TestBackgroundIoPolicy` now requires the explicit maintenance lane (`isCoverTileMembershipMaintenancePending`, `repairCoverTileMembershipBatch`, open-time marker arm, and background-worker repair loop) to stay wired for pre-index libraries.
- 2026-06-11: `py scripts\qa_probe_cover_rebuild_8082.py --scan-root "H:\Sound effect\综合\8082"` re-ran successfully after the maintenance-lane patch; `.openclaw\reports\qa_probe_cover_rebuild_8082\harness_multi\report.json` shows `3897` imported files, `44` cover groups, `cover_tile_membership=2172`, `display_count=44`, `visible_ready=28`, and `fallback_items=0`.
- 2026-06-11: `getFileIdsForCoverTileMembership()` now retires the legacy audio-field fallback completely after canonical membership + self-heal; unresolved read-only opens no longer revive legacy `embedded_cover_key` / `folder_cover_key` fields.
- 2026-06-11: `$env:AURA_TEST_DATABASE_FEATURES_CASE='cover_tile_membership'; build_dbg\Debug\TestDatabaseFeatures.exe | Select-String "read-only open no longer revives legacy audio cover-key fields"` confirmed `openReadOnly()` no longer revives the removed fallback when `cover_tile_membership` rows are absent.
- 2026-06-11: `TestBackgroundIoPolicy` now requires the tile-membership contract to keep `embedded_cover_key IN (...)` / `folder_cover_key IN (...)` out of `getFileIdsForCoverTileMembership()`, proving the legacy field fallback is fully retired instead of merely gated by a branch.
- 2026-06-11: `py scripts\qa_probe_cover_rebuild_8082.py --scan-root "H:\Sound effect\综合\8082" --work-db ".openclaw\reports\qa_probe_cover_rebuild_8082\harness_multi_20260611_042947_full_retire\auro_8082.db" --report-json ".openclaw\reports\qa_probe_cover_rebuild_8082\harness_multi_20260611_042947_full_retire\report.json"` passed after full fallback retirement; the fresh report shows `3897` imported files, `44` cover groups, `cover_tile_membership=2172`, `display_count=44`, `visible_ready=28`, and `fallback_items=0`.

## Change Gate

- [x] Refresh Graphify main and audit graphs with `python tools/graphify_auto_refresh.py --once`.
- [x] Re-run this module's TDD / QA proof.
- [x] Review module-relevant `UNKNOWN_BLOCKING` entries for this bridge cut; current blockers are broad graph-parser noise, not a new cover bridge dependency.
- [x] Update this contract whenever a legacy truth lane is reclassified as `retire now`, `compat bridge`, or `rollback scaffold`.

## Decision Log

| Date | Decision | Reason | Decider |
| --- | --- | --- | --- |
| 2026-06-10 | Slice 5 starts only after Slice 4 integrated baseline is green in `harness_multi`. | retirement without one trusted main-path proof lane would make regressions impossible to attribute cleanly. | Product / Engineering |
| 2026-06-10 | First retirement focus is the visible `SearchFallback` / legacy display-truth lane, not immediate DB field deletion. | deleting DB compatibility first would hide the more immediate product-facing dual-truth boundary in the render path. | Product / Engineering |
| 2026-06-10 | Preliminary ?|... identity remains an explicit compatibility semantic and is not equivalent to old unstable supported-state fallback. | the product design preserves preliminary identity during incomplete canonical resolution, so Slice 5 must not erase it accidentally. | Product / Engineering |
| 2026-06-11 | Real-library `8082` import + rebuild report is the canonical rebuild proof for Slice 5 closure, with the fixture-switch rebuild lane retained only as a diagnostic fallback. | the real package proved import `3897`, rebuild `3897/3897`, `44` cover groups, and `fallback_items=0`, while the fixture-switch lane was not stable enough to be the only proof. | Engineering |
| 2026-06-11 | The ambiguous file-path cover-key reverse lookup was renamed before any deeper deletion. | the remaining DB reverse lookup was still needed by artwork drag/drop and embedded-art recovery, so the safe retirement move was explicit quarantine rather than silent removal. | Engineering |
| 2026-06-11 | Playlist artwork-drop temporarily used a playlist-ID-only bridge, separating playlist membership from file-path recovery before the old file-path bridge deletion. | separating playlist IDs from file-path recovery reduced the old bridge blast radius and gave the deletion slice a smaller target. | Engineering |
| 2026-06-11 | The file-path cover-key bridge is deleted after embedded recovery stops using DB same-key path probing. | replacement cover loading already has DB thumb cache and representative `filePathUtf8`; same-key file-path probing kept a hidden legacy truth lane alive. | Engineering |
| 2026-06-11 | Playlist artwork-drop production paths now use `Database::getFileIdsForCoverTileMembership`; the old playlist compatibility bridge name is retired. | the remaining user action is semantically tile membership, not a legacy cover-key escape hatch, while DB cover-key field deletion remains a separate retained-debt slice. | Engineering |
| 2026-06-11 | Tile membership backing now expands through registry `normalized_key` / `cover_phash` equivalent keys before reading file ids. | playlist artwork-drop should follow the same replacement cover group semantics as wall/facet visual clustering, not a single legacy key equality lookup. | Engineering |
| 2026-06-11 | Tile membership now has a canonical `cover_tile_membership` DB index maintained by registry rebuild/delta paths; legacy audio cover-key fields are only a retained-debt fallback when the index is missing or stale. | this moves the normal replacement membership truth out of `audio_files.embedded_cover_key` / `folder_cover_key` without deleting rollback scaffolding prematurely. | Engineering |
| 2026-06-11 | Writable legacy DBs now self-heal missing `cover_tile_membership` rows on demand for the requested tile, and specific-cover filter entry points pre-arm that repair on the primary writable DB before async worker refresh begins. | the real runtime search/filter worker is read-only, so repairing on the entry path preserves canonical membership truth without reviving the retired legacy audio-field fallback. | Engineering |

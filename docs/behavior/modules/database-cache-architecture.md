# Database / Cache Architecture Behavior Contract

## Header

**ID:** BDD-DATABASE-CACHE-ARCHITECTURE
**Module:** Database / Cache Architecture
**Status:** Active
**Priority:** P0
**Owner:** Engineering
**Created:** 2026-06-10
**Last Updated:** 2026-07-11

## Product Behavior

**User promise:** Aris keeps user library truth separate from rebuildable cache data, so large sound libraries stay fast, compactable, recoverable, and explainable without risking source audio or user-created metadata.
**Primary workflow:** User opens or scans a library, searches and filters immediately, sees covers and waveforms progressively, audits cache size, compacts or rebuilds cache data when needed, and can still deliver sounds without cache maintenance changing source audio.
**Non-goals:** This contract does not require copying Soundminer private schemas, making startup run full-library rebuilds, or moving every cache and activity surface in one migration.

## 中文语义

**用户承诺：** Aris 的主库保存不可随意丢失的库事实，封面、波形、分析特征、快照和行为历史按可解释的边界存放；缓存可以重建或清理，但不能破坏搜索、试听、元数据、播放列表或源音频安全。
**核心流程：** 用户打开或扫描大库后，主库先提供可搜索、可筛选、可恢复的结果；封面和波形通过 sidecar/cache 状态渐进补齐；维护、压缩和迁移有诊断证据，不在启动和交互路径做无界工作。
**非目标：** 本契约不要求照搬 Soundminer 的 SQLite 表名、私有字段或动态建表方式，也不要求一次升级完成持久化查询快照和独立 activity DB 的全部实现。

## BDD Scenarios

### Readiness Assessment

- Given 已满足以下前置条件：the Soundminer benchmark suggests separate main database, media cache databases, view snapshots, and workflow/activity logs
- When 执行以下操作：Aris product management checks current interfaces before database upgrade planning
- Then 应得到以下结果：the first upgrade slice is supported by existing `Database::open`, `attachSidecarDatabases`, `createSidecarTables`, `getCacheSizeReport`, `compactCachePackage`, `cover_cache_registry`, `blobs_db.cover_thumbs`, `waveforms_db.waveforms`, `facet_query_cache`, `backfill_pending`, and scan foreground completion interfaces
- And durable query/view snapshot storage plus a standalone activity/history database remain follow-up interface gaps rather than accepted as already implemented

### Main Truth Versus Rebuildable Cache

- Given 已满足以下前置条件：a library contains user-visible file metadata, playlists, tags, search indexes, cover references, waveform blobs, and generated thumbnails
- When 执行以下操作：Aris opens, migrates, backs up, repairs, or destroys a database package
- Then 应得到以下结果：main library truth stays in the main DB while large rebuildable blobs live in attached sidecars or explicit cache tables, and package diagnostics report the main, blob, waveform, WAL, and cache contributors separately

### Cache State And Recovery

- Given 已满足以下前置条件：waveform, cover, or scan-derived cache work is pending, done, failed, stale, or cancelled
- When 执行以下操作：background workers, repair center, scan completion, or UI readiness checks observe the database
- Then 应得到以下结果：every cache family exposes a named state, terminal failure path, retry/rebuild path, and diagnostic proof instead of forcing synchronous first-use work on the UI thread

### Snapshot And Activity Follow-Up

- Given 已满足以下前置条件：a user returns to a previous search, cover filter, result tab, audition, or export workflow
- When 执行以下操作：Aris restores context after startup or DB switch
- Then 应得到以下结果：the database layer can persist lightweight `query_view_snapshots` entries for search/view context without storing large result payloads
- And the database layer can persist lightweight `activity_log` entries for workflow history without storing source audio or generated cache blobs
- And existing revision-based tab refresh, search workspace state, user action log, and export diagnostics may protect current in-session behavior
- But UI startup/DB-switch consumption of snapshots, UI/export writes into `activity_log`, and dedicated `transfer_log` / sidecar policy remain required follow-up interfaces before claiming Soundminer-style snapshot/activity parity

### Migration And Startup Safety

- Given 已满足以下前置条件：an existing Aris database has legacy waveform blobs or older cover cache rows
- When 执行以下操作：a new Aris build opens the database
- Then 应得到以下结果：migration moves or invalidates rebuildable cache data without deleting user metadata, does not run unbounded full-library maintenance on startup, records version gates in metadata, and keeps worker-thread connections from running primary-only migrations

### Non-Functional

- Given 已满足以下前置条件：a 3M-file target library
- When 执行以下操作：Aris evaluates this database/cache upgrade
- Then 应得到以下结果：the smallest shippable slice must preserve search recall, keep startup bounded, support cancellation or staged maintenance for cache rebuilds, expose package-size diagnostics, and define separate proofs for search, scan, cover, waveform, startup, and delivery surfaces

## Interface Binding

| Interface / Entry | Kind | Source | Expected behavior |
| --- | --- | --- | --- |
| `Database::open` | DB | Graphify + source scan | Opens the main database, applies safe PRAGMAs, attaches sidecars, and runs primary-only migrations without unbounded cache rebuild |
| `attachSidecarDatabases` | DB | source scan | Maps rebuildable blobs and waveforms into sidecar databases tied to the main DB package |
| `createSidecarTables` | DB | source scan | Creates and upgrades `blobs_db.cover_thumbs`, `waveforms_db.waveforms`, `external_waveforms`, and waveform cache metadata |
| `migrateFromSingleDatabase` | DB | source scan | Moves legacy waveform blobs out of `audio_files` into `waveforms_db` without changing user-facing metadata |
| `getCacheSizeReport` | DB | source scan | Separates main DB, blob DB, waveform DB, WAL/SHM, and major cache table size evidence |
| `compactCachePackage` | DB | source scan | Provides explicit manual compaction/maintenance instead of hidden startup VACUUM work |
| `RepairCenterSnapshot` | API | source scan | Makes database package health, missing roots, failed cache rows, and repair warnings visible |
| `cover_cache_registry` / `cover_thumbs` | DB | source scan + cover contract | Keeps cover identity/reference separate from thumbnail bytes and supports rebuildable cover cache maintenance |
| `waveforms_db.waveforms` | DB | source scan + waveform contract | Stores waveform blobs and cache state outside the main library truth table |
| `facet_query_cache` | DB | source scan + search contract | Caches expensive facet payloads without changing search/list/count semantics |
| `query_view_snapshots` | DB | BDD + source scan | Persists lightweight search/view context in the main DB while keeping result rows rebuildable |
| `Database::saveQueryViewSnapshot` / `loadQueryViewSnapshot` | API | source scan + test | Saves, reloads, updates, prunes, and deletes query/view snapshots through prepared statements |
| `activity_log` | DB | BDD + source scan | Persists lightweight workflow history in the main DB with bounded query and prune support |
| `Database::appendActivityLog` / `getRecentActivityLog` | API | source scan + test | Appends, filters, reloads, and prunes workflow activity rows through prepared statements |
| `backfill_pending` / `scan_foreground_completion` | DB | source scan + scan contract | Persists derived-work debt and scan completion gates with explicit terminal states |
| `UserActionLog::log` | diagnostics | source scan + UI orchestration contract | Provides current in-memory/diagnostic action history but is not a durable activity DB substitute |
| export diagnostics | diagnostics | delivery contract | Proves export lifecycle events today while DB-backed transfer history remains follow-up |

## Graphify Binding

**Source graph:** `graphify-out/main/interface-call-graph.json` and `graphify-out/main/audit-call-graph.json`
**Impact target(s):** `Database::open`, `attachSidecarDatabases`, `createSidecarTables`, `getCacheSizeReport`, `compactCachePackage`, `RepairCenterSnapshot`
**Audit classification requirement:** DB package and cache sidecar unknowns must be reviewed before implementation because startup, migration, search, cover, waveform, and user data safety share the blast radius.
**Dynamic calls:** review SQLite helper wrappers, package file operations, worker callback paths, and diagnostics emitters.
**Unknown calls:** review `Database.cpp` and `WindowsMain.cpp` `UNKNOWN_BLOCKING` entries touching database open, sidecar attach, cache compaction, search/cover/waveform readiness, and export/activity logging.

## TDD / QA Proof

| Proof ID | Kind | Artifact / command / event | Pass signal | Status |
| --- | --- | --- | --- | --- |
| TEST-DB-SIDECAR-CACHE | integration | `TestDatabaseFeatures` | sidecar/cache tables, query/view snapshots, activity log rows, cover/facet cache, and DB package behaviors stay readable and bounded | active |
| TEST-STARTUP-REPAIR-PACKAGE | integration | `TestStartupReconciliation` | repair snapshot separates package metrics, missing roots, failed waveform rows, and cover cache health | active |
| TEST-SCAN-DERIVED-DEBT | integration | `TestScanWaveformCacheContract` | scan-derived waveform/cover/backfill debt is durable, cancellable, and terminal-state driven | active |
| TEST-SEARCH-WORKSPACE-SNAPSHOT | integration | `TestSearchWorkspace` | current workspace snapshot behavior remains correct while durable query snapshots are deferred | active |
| TEST-USER-ACTION-LOG | diagnostics | `TestUserActionLog` | current action-log ordering is preserved for QA/debug evidence | active |
| TEST-DELIVERY-EXPORT-DIAG | integration | `TestExportRegionPreserve` | export lifecycle diagnostics remain available before DB-backed transfer log exists | active |
| DIAG-DB-PACKAGE | diagnostics | repair center / cache size report | main/blobs/waveforms/WAL/cache bytes are visible and explainable | active |

## Missing Coverage

- Missing proof: UI startup and DB-switch flows do not yet consume `query_view_snapshots`; the current proof is database-layer persistence only.
- Missing proof: UI/export flows do not yet write into `activity_log`; no standalone `activity.sqlite` / `transfer_log` sidecar policy exists yet.
- Missing proof: no single end-to-end migration test proves all Soundminer-inspired storage tiers together on a large representative package.
- Residual risk: `cover_thumbs` still has a compatibility table in the main schema as well as `blobs_db.cover_thumbs`; future cleanup must prove old databases and worker connections remain safe.
- Human-only verification: whether database package controls feel understandable enough for non-engineer sound designers.

## Change Gate

- [x] Refresh Graphify main and audit graphs with `python tools/graphify_auto_refresh.py --once`.
- [x] Re-run database/cache architecture mapped proofs or mark unrun proof as `INCOMPLETE_VERIFICATION`.
- [x] Review `Database.cpp`, `WindowsMain.cpp`, cover, waveform, search, scan, and delivery `UNKNOWN_BLOCKING` entries touched by the change.
- [x] Update this contract if main DB truth, sidecar layout, cache state vocabulary, snapshot persistence, activity logging, package repair, or startup migration semantics change.
- [x] Do not claim Soundminer-style parity until query/view snapshots and DB-backed activity/transfer history have durable interfaces and tests.

## Decision Log

| Date | Decision | Reason | Decider |
| --- | --- | --- | --- |
| 2026-06-10 | Current Aris interfaces support the first database/cache architecture upgrade slice. | Main DB plus blobs/waveforms sidecars, cache states, package metrics, compaction, and derived-work queues already exist. | Product / Engineering |
| 2026-06-10 | Durable query/view snapshots and standalone activity DB are follow-up gaps. | Current workspace/action/export evidence is useful but does not equal persistent snapshot/activity storage. | Product / Engineering |
| 2026-06-10 | BDD is recorded as a behavior contract before implementation. | Database/cache upgrades cross search, scan, cover, waveform, startup, and delivery, so discussion-only wording would be too fragile. | Product / Engineering |
| 2026-06-10 | Add main-DB `activity_log` as the first durable activity-history layer. | It proves the DB-backed workflow-history interface without changing UI/export behavior or committing to a high-volume sidecar yet. | Product / Engineering |

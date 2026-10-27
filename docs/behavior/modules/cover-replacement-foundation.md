# Cover Replacement Foundation Behavior Contract

**ID:** BDD-COVER-REPLACEMENT-FOUNDATION
**Module:** Cover Replacement Foundation
**Status:** Active
**Priority:** P1
**Owner:** Engineering
**Created:** 2026-06-10
**Last Updated:** 2026-07-11

## Product Behavior

**User promise:** The replacement cover system keeps one explicit identity vocabulary for preliminary, legacy, and canonical cover truth before broader wall/filter integration begins.
**Primary workflow:** Establish the selected storage compatibility mode, classify cover identity state, and normalize comparison keys without mutating the current Aris cover runtime.
**Non-goals:** Live wall rendering, preview loading, database migration execution, cover thumbnail writes, and retirement of current Aris cover behavior.

## 中文语义

**用户承诺：** 在真正替换 Aris 封面墙之前，新的封面系统先把 preliminary identity、legacy key、canonical identity 这三类基础语义钉死，后续墙面、过滤和预览都基于同一套身份规则继续长出来。
**核心流程：** 先固定 `DB-first dual storage` 兼容模式，再用纯函数判断一个封面身份是 preliminary / legacy / canonical，并生成稳定的比较 key；这个阶段不改变当前 Aris 的真实封面展示路径。
**非目标：** 不直接改动当前封面墙、不执行数据库迁移、不做封面缩略图写入、不接管预览或主界面显示。

## BDD Scenarios

### Happy Path

- Given 已满足以下前置条件：the replacement program starts from current Aris DB-first cover truth
- When 执行以下操作：Slice 1 foundation is initialized
- Then 应得到以下结果：the selected compatibility mode remains `DbFirstDualStorage`.

- Given 已满足以下前置条件：a cover identity key enters replacement-side comparison logic
- When 执行以下操作：the key is preliminary, legacy, or canonical
- Then 应得到以下结果：the classification stays explicit and stable for later wall/filter/preview reuse.

### Edge Cases

- Given 已满足以下前置条件：a temporary `?|folder` identity exists because durable cover truth is not ready yet
- When 执行以下操作：the identity is normalized for comparison
- Then 应得到以下结果：it stays `?|folder` and is not erased or converted into a different semantic bucket.

- Given 已满足以下前置条件：a legacy `F|<path>` key and a canonical `F|PATH|<path>` key point to the same path
- When 执行以下操作：comparison keys are normalized
- Then 应得到以下结果：they resolve to the same comparison form.

### Failure / Recovery

- Given 已满足以下前置条件：broader replacement slices are not ready yet
- When 执行以下操作：this foundation module is used
- Then 应得到以下结果：it must remain pure contract logic and not mutate current DB or UI truth.

### Non-Functional

- Given 已满足以下前置条件：the foundation helpers may be called in hot paths later
- When 执行以下操作：classification or normalization runs
- Then 应得到以下结果：the work stays deterministic, allocation-light, and free of synchronous DB or filesystem access.

## Interface Binding

| Interface / Entry | Kind | Source | Expected behavior |
| --- | --- | --- | --- |
| `CoverStorageCompatibilityMode` | API | Product / current task | Encodes the approved compatibility direction for replacement slices. |
| `kSelectedCoverStorageCompatibilityMode` | API | Product / implementation plan | Must remain `DbFirstDualStorage` until a later explicit decision changes the program contract. |
| `CoverIdentityState` | API | Product / source | Distinguishes preliminary, legacy, canonical, and unknown identity states. |
| `CoverMigrationBridgeState` | API | Product / design spec | Marks that replacement records remain rollback-aware rather than destructive. |
| `CoverIdentityRecord` | API | Product / source | Carries identity key, source path, optional canonical id, and migration bridge state. |
| `IsPreliminaryCoverIdentity` | API | Product / source | Recognizes preliminary identities without collapsing them. |
| `IsLegacyCoverIdentityKey` | API | Product / source | Distinguishes old folder-path identities from canonical PATH and hash forms. |
| `ClassifyCoverIdentityState` | API | Product / source | Maps keys into the explicit replacement identity buckets. |
| `NormalizeIdentityKeyForComparison` | API | Product / source | Keeps preliminary identities stable and makes legacy/canonical path forms comparable. |

## Graphify Binding

**Source graph:** `graphify-out/main/interface-call-graph.json` and `graphify-out/main/audit-call-graph.json`
**Impact target(s):** `CoverStorageCompatibilityMode`, `CoverIdentityState`, `NormalizeIdentityKeyForComparison`
**Audit classification requirement:** `UNKNOWN_BLOCKING` entries touching this module must be reviewed before completion.
**Dynamic calls:** review `graphify-out/main/DYNAMIC_CALLS.md` for this module.
**Unknown calls:** review `graphify-out/main/UNKNOWN_CALLS.md` for this module.

## TDD / QA Proof

| Proof ID | Kind | Artifact / command / event | Pass signal | Status |
| --- | --- | --- | --- | --- |
| `TEST-COVER-REPLACEMENT-FOUNDATION-CONTRACT` | contract | `TestCoverReplacementFoundation` | pass | active |
| `PROOF-COVER-ROLLBACK` | source / contract | `src/CoverReplacementFoundation.*` + this contract | rollback-ready dual-storage state remains explicit | active |
| `PROOF-COVER-PRELIMINARY-IDENTITY` | contract | `TestCoverReplacementFoundation` | `?\|...` remains preliminary and comparison-stable | active |

## Missing Coverage

- Missing proof: current preview parity and live wall/filter parity are not covered in this module and belong to later slices.
- Suspected stale proof: none yet; this module is new.
- Human-only verification: none for Slice 1 foundation itself.

## Change Gate

- [x] Refresh Graphify main and audit graphs with `python tools/graphify_auto_refresh.py --once`.
- [x] Re-run this module's TDD / QA proof.
- [x] Review module-relevant `UNKNOWN_BLOCKING` entries.
- [x] Update this contract if product behavior or interface binding changed.

## Decision Log

| Date | Decision | Reason | Decider |
| --- | --- | --- | --- |
| 2026-06-10 | Lock Slice 1 foundation to `DB-first dual storage`. | Current Aris wall/filter/thumb truth is still DB-first; additive compatibility is the smallest safe replacement entry. | Product / Engineering |
| 2026-06-10 | Preserve `?|...` as a first-class preliminary identity. | Existing sparse/fallback cover behavior depends on temporary identity surviving until durable truth exists. | Product / Engineering |

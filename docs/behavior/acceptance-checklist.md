# Aris BDD / TDD 验收 Checklist

**Status:** Active baseline
**Created:** 2026-06-03
**Last Updated:** 2026-06-05

这份清单不是替代行为契约，而是把行为契约变成更容易签字的验收口径。

目标很直接:

- 改动后先知道要看哪个模块
- 立刻知道该跑什么命令
- 明确什么信号才算通过
- 明确哪些点还需要人眼或人耳确认

## 使用顺序

### 1. 确认验收范围

先在 [`module-index.md`](module-index.md) 找到这次改动对应的模块 key，再打开对应 `docs/behavior/modules/*.md` 契约。

### 2. 先跑全局前置 gate

```powershell
py tools/behavior_contract_check.py
py tools/graphify_auto_refresh.py --once
```

通过信号:

- `behavior_contract_check` 通过
- Graphify main + audit graph 刷新成功
- 如果当前改动模块命中了新的 `UNKNOWN_BLOCKING`，必须在交付说明里解释

### 3. 跑模块回归

优先命令:

```powershell
py tools/behavior_regression_suite.py --module <key> --run
```

如果是跨模块或发版前验收，跑:

```powershell
py tools/behavior_regression_suite.py --all --run
```

### 4. 对照“通过信号”签字

每个模块都要同时看三层:

- BDD 行为是否仍成立
- TDD / diagnostics proof 是否真的跑过
- Human-only 项是否仍被诚实地标成待人工确认

没有跑 proof，或还有未解释的相关入口没验，结论只能写 `INCOMPLETE_VERIFICATION`。

## 全量验收

适用场景:

- 大范围重构
- 多模块联动修复
- 准备做系统性回归
- 用户明确要求“全量跑一次”

命令:

```powershell
py tools/behavior_contract_check.py
py tools/graphify_auto_refresh.py --once
py tools/behavior_regression_suite.py --all --run
```

通过信号:

- 10 个已覆盖模块全部通过
- 没有把未跑 proof 伪装成通过
- 如果 `UNKNOWN_BLOCKING` 仍存在，交付说明里要写明它是不是和本次改动无关

## 模块签字表

| 模块 key | 先看什么 | 跑什么 | 通过信号 | 仍需人工确认 |
| --- | --- | --- | --- | --- |
| `search-query` | `docs/behavior/modules/search-query.md` | `py tools/behavior_regression_suite.py --module search-query --run` | `TestSearchSemanticMatrix`、`TestSearchQueryTrigram`、`TestSearchRecall` 通过；搜索语义矩阵和 trigram 召回没有回归 | visual chip edit ergonomics |
| `search-execution` | `docs/behavior/modules/search-execution.md` | `py tools/behavior_regression_suite.py --module search-execution --run` | `TestDbSearchFacets`、`TestTrigramE2E`、`TestBrowserSearch` 通过；`Database.searchFiles` 路径仍满足语义和性能约束 | perceived search relevance |
| `scan-indexing` | `docs/behavior/modules/scan-indexing.md` | `py tools/behavior_regression_suite.py --module scan-indexing --run` | `TestScanRenameQa`、`TestBackfillRecoveryWorker`、`TestScanWaveformCacheContract` 通过；`Scan.*` summary/cache report 出现；无 rollback / flush failure | real-library `极速模式` final signoff: follow `docs/behavior/manual-acceptance-checklist.md` under `scan-indexing`; verify completion does not stall and first scroll / first cover-wall screen stay usable |
| `waveform` | `docs/behavior/modules/waveform.md` | `py tools/behavior_regression_suite.py --module waveform --run` | `TestScanWaveformCacheContract`、`TestWaveformPresentationPolicy`、`TestImportPrecomputeWaveform` 通过；`MiniWave` / `ScanWaveformCacheReport` runtime assertions 通过 | real-library `极速模式` final signoff: follow `docs/behavior/manual-acceptance-checklist.md` under `waveform`; verify first unseen-file selections still resolve the current-file main waveform without obvious drag |
| `cover` | `docs/behavior/modules/cover.md` | `py tools/behavior_regression_suite.py --module cover --run` | `TestCoverPathNorm`、`TestCoverLaneOrder`、`TestCoverClickPerf`、`TestCoverDiagnostics` 通过；`CoverLane` / `Cover.TextureUploaded` runtime assertions 通过 | artwork visual correctness |
| `playback-library-lifecycle` | `docs/behavior/modules/playback-library-lifecycle.md` | `py tools/behavior_regression_suite.py --module playback-library-lifecycle --run` | `TestSwitchDatabaseBoundary`、`TestStartupReconciliation`、`TestOggPlaybackSupport` 通过；`Audio.loadFile` 和 switch-stage diagnostics 仍成立 | audible playback continuity and device behavior |
| `metadata-tags` | `docs/behavior/modules/metadata-tags.md` | `py tools/behavior_regression_suite.py --module metadata-tags --run` | `TestMetadataFields`、`TestTagSystem`、`TestTagVocabularyMapper`、`TestMultiTabSync` 通过；metadata patch success/rejection diagnostics assertions 通过 | field editor UX |
| `playlist-organization` | `docs/behavior/modules/playlist-organization.md` | `py tools/behavior_regression_suite.py --module playlist-organization --run` | `TestSubPlaylists`、`TestPlaylistFolderTree`、`TestSetPlaylistParent`、`TestPlaylistQuickBarMotion` 通过；playlist parent success/reject diagnostics assertions 通过 | drag/drop ergonomics |
| `delivery-export` | `docs/behavior/modules/delivery-export.md` | `py tools/behavior_regression_suite.py --module delivery-export --run` | `TestExportRegionPreserve`、`TestProcessedFileExport`、`TestVoiceDeliveryAcceptance` 通过；export start/completion/failure diagnostics assertions 通过 | DAW/team handoff ergonomics |
| `settings-startup` | `docs/behavior/modules/settings-startup.md` | `py tools/behavior_regression_suite.py --module settings-startup --run` | `TestAppSettingsDefaults`、`TestStartupReconciliation`、`TestProductEdition`、`TestSettingsEncryption` 通过；startup branch diagnostics/source-contract assertions 通过 | perceived startup clarity |
| `cover-replacement-foundation` | `docs/behavior/modules/cover-replacement-foundation.md` | `py tools/behavior_regression_suite.py --module cover-replacement-foundation --run` | `TestCoverReplacementFoundation` 通过；`DB-first dual storage`、preliminary identity、legacy/canonical comparison contract 保持成立 | none for this slice; preview/wall parity belongs to later slices |
| `cover-replacement-browse-surface` | `docs/behavior/modules/cover-replacement-browse-surface.md` | `py tools/behavior_regression_suite.py --module cover-replacement-browse-surface --run` | `TestCoverBrowseSurfaceState`、`TestDatabaseFeatures`、`TestCoverClickPerf`、`TestBackgroundIoPolicy` 通过；browse-surface baseline regression 与继承 Slice 2 truth contract 保持成立；targeted browse-surface report `.openclaw/reports/qa_probe_cover_browse_surface/slice3_targeted/report.json` 已证明 keyword/tree/path/cover/slider/preview 同一 browse-surface scope contract；before/after screenshots 已证明 wall readability 与 top/bottom info clarity | only live interaction feel remains human-only: wheel/slider feel on a real large library |
| `cover-replacement-retirement` | `docs/behavior/modules/cover-replacement-retirement.md` | `py tools/behavior_regression_suite.py --module cover-replacement-retirement --run` | retirement source/DB contract tests通过；retirement probe 证明 no-filter / keyword / path / cover prime-repeat、preview parity、rollback、rebuild 均保持成立；legacy fallback audit 已明确 supported-library 状态不再依赖隐藏旧 truth | real post-retirement visual parity and rebuild recovery smoke if probe cannot fully prove them |

## 签字模板

适合放在任务总结、QA Ledger、需求卡 closure、或对话收口里:

```md
## 验收结论

- 模块: `waveform`, `scan-indexing`
- 运行命令:
  - `py tools/behavior_contract_check.py`
  - `py tools/graphify_auto_refresh.py --once`
  - `py tools/behavior_regression_suite.py --module waveform --run`
  - `py tools/behavior_regression_suite.py --module scan-indexing --run`
- 自动通过信号:
  - waveform tests + runtime diagnostics assertions passed
  - scan tests + `Scan.*` summary/cache report passed
- 人工待确认:
  - follow `docs/behavior/manual-acceptance-checklist.md` under `scan-indexing` and `waveform`
  - confirm real-library `极速模式` completion does not stall the first scroll, first cover-wall screen, or current-file main waveform lane
- 结论:
  - `PASS`, 若人工项不阻塞当前代码继续推进
  - `INCOMPLETE_VERIFICATION`, 若相关 proof 未跑或相关入口未验证
```

## 什么时候不能签 PASS

- 只跑了单个 ctest，没有跑模块 suite
- 没跑 `behavior_contract_check`
- 没刷新 Graphify
- 改动命中了相关模块，但没有回到对应 contract 看 proof
- diagnostics 应该是自动断言，却只写“理论上会出现”
- Human-only 项被静默省略

这种情况结论必须老老实实写 `INCOMPLETE_VERIFICATION`。

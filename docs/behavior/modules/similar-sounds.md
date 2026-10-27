# Similar Sounds Behavior Contract

**ID:** BDD-SIMILAR-SOUNDS
**Module:** Similar Sounds
**Status:** Active
**Priority:** P1
**Owner:** Airs team
**Created:** 2026-07-16
**Last Updated:** 2026-10-22

## Product Behavior

**User promise:** A sound designer can open one Similar Sounds window from the selected file, request acoustically similar results, and audition or chain-search a result without losing the current library selection.

**Primary workflow:** Open the information-area entry, inspect the current reference, choose a preset, request results, select a row locally, and audition it using the global single-click/double-click preference or Space. Clicking Find with a local row selected makes that row the next window-local reference.

**Non-goals:** This module does not replace the main result list, own global selection, persist invalid feature vectors, or treat automated checks as proof of visual or listening quality.

## 中文语义

**用户承诺：** 音效师可从当前选中文件打开唯一的相似音效窗口，查找声学相近结果，并在不改变主列表选择的情况下试听或继续查找。

**核心流程：** 从信息区入口打开单实例窗口；需要时在查找栏下方开始或取消批量分析，并查看索引数量和进度。确认当前音效，选择预设并显式查找；结果到达后在窗口内选行，按全局单击/双击播放偏好或按 Space 试听；选中结果后再次显式查找可在窗口内链式查找。

**非目标：** 不把相似结果写回主搜索列表，不静默保存无效或全零特征，不以自动化测试代替视觉与听感人工验收。

## BDD Scenarios

### Single instance and explicit request

- Given 当前主列表存在有效单选音效，且相似音效窗口尚未打开
- When 用户连续点击两次信息区相似音效入口
- Then 只打开并聚焦一个非模态窗口，复用同一个 workflow 与窗口状态，不创建重复实例

- Given 当前参考音效可读，窗口处于 Ready 状态
- When 用户选择预设并点击 Find
- Then 提交包含当前 database、reference、preset 与 topK 的不可变请求，并在完成后只更新窗口结果

### Selection and freshness

- Given 窗口正在显示上一参考音效的结果
- When 主列表 selection change 被观察到且用户没有点击 Find
- Then 清空旧结果并重置本地 selected row，但不得自动提交新搜索

- Given 批量声学特征分析正在进行，窗口可能保留上一轮结果
- When 分析状态从空闲变为进行中
- Then 立即使旧 snapshot 失效，Find 显示“正在分析特征…”并禁用；分析结束后仍需用户显式重新 Find

- Given generation N 的搜索仍在执行，用户又提交 generation N+1
- When N 与 N+1 以任意顺序完成
- Then 只允许最新 generation N+1 更新 snapshot，旧完成不得复活旧结果或错误

### Row interaction

- Given 结果表存在一行
- When 用户单击该行
- Then 更新窗口本地 selected row，并按全局 PlaybackMode 执行单击播放策略，不改变主列表 selection 或全局 searchResults

- Given 结果表存在一行且该行已在窗口内选中
- When 用户显式点击 Find
- Then 该行成为窗口局部 reference 并提交新请求；不改变主列表 selection、selectedIndicesSet 或 currentFileId

- Given 一条结果尚未载入播放器
- When 用户按全局单击/双击播放偏好、双击该行，或窗口聚焦且用户按 Space
- Then 使用现有播放与波形路径试听该行；已载入且播放中的行暂停，暂停行恢复，播放结束后从零开始

### Failure and recovery

- Given 参考路径不可读、解码失败或参考特征无效，数据库中也没有有效参考向量
- When SimilarityQueryService 执行请求
- Then 返回 ReferenceUnreadable，结果为空，不持久化合成的全零特征，并在窗口表体显示一行可理解的本地化错误

- Given 候选缓存含非有限值、损坏 blob 或数据库不可用
- When 查询服务刷新或读取缓存
- Then 拒绝无效输入并返回明确错误，不发布部分结果；后续有效请求仍可重新建立缓存

### Approved window contract

- Given 相似音效独立窗口可见
- When 窗口完成一帧渲染
- Then 显示批准的五层：标准标题栏、当前音效与预设及 Find 操作行、索引与批量分析区、结果标题与数量、五列结果表格

- Given 结果表格可见
- When 用户检查表头
- Then 列顺序固定为 File Name、Waveform、Duration、Format、Similarity；索引数量只在表格上方的分析区显示，不增加结果列

### 批量分析面板迁移（RQ-20261022-01）

- 索引数量、开始批量分析、进度条、取消和完成提示统一位于相似音效窗口，浮动和停靠使用同一内容入口。
- 完成提示只在本次非空分析完成且全部文件已索引后出现，本次打开期间保留；下次打开窗口时隐藏，索引数量持续保留。空库、打开已有完整索引库或取消任务不显示成功提示，不使用统一倒计时。
- AI 语音识别窗口移除“相似搜索”页签，语音后台任务摘要保留；预设复用相似音效顶部已有控件。
- 开始分析沿用原任务，立即作废旧搜索结果；关闭窗口不取消任务，重新打开显示真实进度。
- 切库时禁止开始分析；切库与任务收尾遵守原有取消和等待规则。索引计数每两秒以同一次查询读取当前音效表，数据库身份改变、窗口重新出现和任务结束时重取，避免目录统计重建期间误报总数为 0。
- 中文、英文、日文复用原有索引状态文案；移除旧标签和重复预设的七个闲置语言键。
- CLI：隔离 QA 包提供 `similar_index`，操作为 `open/close/start/cancel/find/state`。`tools/qa_similar_index.py --action state` 可直接查询；复杂参数用 `--command-file` 传 JSON 文件。该控制入口只在固定数据根目录的预览构建中启用。
- 2026-10-22：完整 HarkenSearch Release 构建成功，8 项 CTest 通过；160 个合成音效完成分析、取消/重启、分析中关窗重开、切库和 50 条相似结果回归。真实窗口截图与命令回执保存在本卡 QA 目录；用户体验验收尚未完成。

### Non-functional

- Given 用户在大结果集中连续改变参考或提交请求
- When 后台 worker 处理重叠任务
- Then 前台保持可交互，过期任务不能阻塞或覆盖最新 snapshot，主线程不执行解码、全表加载或 O(n) 排序

## Interface Binding

| Interface / Entry | Kind | Source | Expected behavior |
| --- | --- | --- | --- |
| `RenderSimilarSoundsWindow` | UI | src/main.cpp | Owns the standalone four-layer window and polls the workflow. |
| `ResolveCurrentSimilarityReference` | UI | src/main.cpp | Resolves the selected/current audio into one immutable reference. |
| `DrawSimilarSoundsActionRow` | UI | src/main.cpp | Renders current reference, preset choice, and Find action. |
| `DrawSimilarSoundsResultsTable` | UI | src/main.cpp | Renders the approved five columns and local row selection. |
| `ToggleSimilarSoundsAudition` | UI | src/main.cpp | Loads, pauses, resumes, or rewinds the selected row for audition. |
| `similarSoundsLocalReference` | UI | src/AppState.h | Owns an explicit Find-click chain reference without writing main-list selection. |
| `SimilarityWorkflow::SetReference` | API | src/SimilarityWorkflow.h | Invalidates old results on reference change without implicitly searching. |
| `SimilarityWorkflow::RequestSearch` | API | src/SimilarityWorkflow.h | Queues an immutable generation-tagged request only when a reference exists. |
| `SimilarityWorkflow::Poll` | worker | src/SimilarityWorkflow.h | Publishes only the latest completion and exposes stable snapshot state. |
| `SimilarityQueryService::Execute` | API | src/SimilarityQueryService.h | Maps unreadable, invalid, and database failures to workflow errors. |
| `FeatureExtractor::extractFromFileChecked` | worker | src/FeatureExtractor.h | Produces finite features or an explicit extraction error. |
| `SimilaritySearch::findSimilar` | API | src/SimilaritySearch.h | Ranks valid finite candidates with bounded scores and stable tie-breaking. |
| `request_start` / `request_failed` / `request_done` | diagnostics | src/SimilarityQueryService.cpp | Records identity, candidate count, cache/query timing, rows, and error. |

## Graphify Binding

**Source graph:** `graphify-out/main/interface-call-graph.json` and `graphify-out/main/audit-call-graph.json`

**Impact targets:** `RenderSimilarSoundsWindow`, `SimilarityWorkflow`, `SimilarityQueryService`, `FeatureExtractor`, and `SimilaritySearch`.

**Expected edge:** `RenderUI` calls `RenderSimilarSoundsWindow`; the renderer synchronizes and polls `SimilarityWorkflow`, which executes the worker-owned query service and ranker without replacing central `searchResults`.

**Audit rule:** module-related `UNKNOWN_BLOCKING` and dynamic calls must be reviewed or recorded honestly before product signoff.

## TDD / QA Proof

| Proof ID | Kind | Artifact / command / event | Pass signal | Status |
| --- | --- | --- | --- | --- |
| TEST-SIMILARITY-SEARCH | contract | `TestSimilaritySearch` | finite bounded scores, stable ranking, invalid vectors rejected | active |
| TEST-FEATURE-EXTRACTOR | unit | `TestFeatureExtractor` | valid audio yields finite features; missing, empty, or cancelled input yields explicit error | active |
| TEST-SIMILARITY-WORKFLOW | integration | `TestSimilarityWorkflow` | reference invalidation, cancellation, immutable input, and latest generation behavior pass | active |
| TEST-SIMILARITY-QUERY-SERVICE | integration | `TestSimilarityQueryService` | display rows preserve order and metadata; failures publish no invalid rows or features | active |
| TEST-SIMILAR-SOUNDS-WINDOW | contract | `TestSimilarSoundsWindowContract` | single window, local selection, waveform seek, playback policy, partial-index gate, five columns, and chain Find hooks remain bound | active |
| TEST-SIMILARITY-LOCALIZATION | contract | `TestLocalization` | Similar Sounds labels, errors, presets, and columns map in both supported languages | active |
| QA-SIMILAR-SOUNDS-VISUAL | QA | approved PNG plus 100% and 150% interaction review | four layers and five columns only; existing typography, layout, spacing, themes, and window lifecycle remain correct | human-only-not-run |
| QA-SIMILAR-SOUNDS-LISTENING | QA | human double-click and Space audition | local result loads, pauses, resumes, rewinds, and sounds correct without replacing the reference | human-only-not-run |

## Missing Coverage

- Automated source and workflow tests do not prove subjective visual density or listening relevance.
- A real GUI pass at 100% and 150% scaling remains human-required.
- A real listening pass remains human-required; ranking and transport tests are not substitutes.
- Module-related Graphify `UNKNOWN_BLOCKING` entries must be reviewed or recorded as follow-up debt.

## Change Gate

- [x] Refresh Graphify main and audit graphs with `python tools/graphify_auto_refresh.py --once` (2026-07-17; 4,153 nodes / 33,574 call edges / 25,722 cross-file edges).
- [x] Re-run the six focused CTest proofs (2026-07-17; 6/6).
- [x] Re-run related database, worker, switch-boundary, and mini-waveform regressions (2026-07-17; 6 additional boundaries passed).
- [x] Review module-relevant Graphify impact and unknown-call evidence (7,720 repository-wide `UNKNOWN_BLOCKING`, retained as explicit follow-up debt).
- [ ] Record visual and listening human-only status honestly.
- [x] Build and report the real Airs package after the full gate passes (2026-07-17; `dist/Airs-Similar-Sounds-20260717-r2-win64.zip`).

## Decision Log

| Date | Decision | Reason | Decider |
| --- | --- | --- | --- |
| 2026-07-16 | Keep Similar Sounds as one non-modal standalone window with four layers and five fixed columns. | It keeps search and result state local while the main sound-library workflow remains available. | Product decision |
| 2026-07-16 | Only an explicit Find action starts a request. | Main-list selection changes must clear stale rows without unexpected background work. | Product decision |
| 2026-07-16 | Treat visual comparison and listening quality as human-only. | Automated contracts prove wiring and state, not visual judgement or audio relevance. | Product and QA |
| 2026-07-17 | Result-row playback follows global PlaybackMode; explicit Find may use the selected row as a window-local reference. | A sound designer should not need an accidental double-click, and repeated Find must not silently reuse the previous reference. Main-list selection remains untouched. | Product review |
| 2026-07-17 | Batch feature extraction invalidates the similarity snapshot and disables Find until completion. | A partial cache of 45/101616 candidates produces provisional, low-quality results that look like stale rows. | QA evidence |


## 第7批下次查找文件名

选中相似结果后，操作区显示下一次 Find 会用的文件名。显示身份和搜索身份分开：有效选中行只改变显示，不调用 SetReference；只有原来的 Find 才把 similarSoundsLocalReference 交给搜索。

### 五项所有权

- identity：显示用有效 similarSoundsSelectedRow 对应行的 fileName。实际搜索仍是 snapshot.reference 与 similarSoundsLocalReference。
- generation：每次绘制动作行读取本次 snapshot 和已有下标。下一次界面刷新就能看到选中名字，不必先点 Find。结果表若在动作行返回后才改选中项，不回头重绘已经返回的动作行，也不新增异步接口。
- cancel：下标为负或越界时回到 reference.displayName；后者为空则用现有 Similar_NoSelection。原来的 Invalidate、关窗、主列表选择变化，以及 Find 后把下标清成 -1，都保持不变。
- stale-result：读取 rows 之前先检查下标上下界。currentName 只指向当次 snapshot，不保留过期下标的指针，也不改既有请求代次或结果发布。
- 生命周期：UI 线程在同一次 Draw 里持有 snapshot。currentName 的 c_str 只活在这次绘制，不写入 AppState，不跨帧保存 this、future 或 ports。没有新 IO、设备、数据库结构、线程池或公共 API。搜索、试听、选库和主列表选择仍按原样运行。

### 验证与回退

源码合同证明 currentName 初始化优先使用选中行 fileName，并且点击 Find 之前没有 SetReference。这不是界面体验、听感或真实引擎搜索。回退只撤 currentName 小段、本测试追加、本附录和台账两行。

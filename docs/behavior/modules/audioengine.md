# AudioEngine Behavior Contract

## Header

**ID:** BDD-AUDIOENGINE
**Module:** AudioEngine
**Status:** Active
**Priority:** P1
**Owner:** Engineering
**Created:** 2026-06-04
**Last Updated:** 2026-07-11

## Product Behavior

**User promise:** AudioEngine applies audition, DSP, channel, and processed-export choices to the current file without mutating source audio or leaking stale engine state across operations.
**Primary workflow:** User loads a file, auditions it, changes mono/solo/pitch/frequency-shift/reverse or device settings, and exports either a processed file or a selected channel.
**Non-goals:** This contract does not define waveform cache publishing policy, DB-switch cleanup semantics, or downstream DAW import behavior after export.

## 中文语义

**用户承诺：** 用户在当前文件上做的播放、DSP、通道、设备和导出选择，只作用于当前文件和目标副本，不会静默改写源音频，也不会把旧的引擎状态冒充成当前状态。
**核心流程：** 用户加载文件、试听，切换 mono/solo、移调/移频、反转、输出设备或导出处理文件时，AudioEngine 必须只应用当前选择的语义，并让导出副本和听到的内容保持一致。旧的保音高变速已从发布程序移除。
**非目标：** 这份合约不定义波形缓存发布策略、跨库清理边界语义，也不定义导入 DAW 后的下游表现。

## BDD Scenarios

### Happy Path

- Given 已满足以下前置条件：a file is loaded into AudioEngine
- When 执行以下操作：the user plays it, toggles mono/solo, adjusts pitch or frequency shift, and exports a processed copy
- Then 应得到以下结果：playback and processed export reflect the current DSP and channel choices for that file only, while the source asset remains unchanged

### Edge Cases

- Given 已满足以下前置条件：the user switches between stereo, surround, and mono-compatible material
- When 执行以下操作：AudioEngine updates channel meters, waveform display source, and solo/mono routing
- Then 应得到以下结果：the active channel count and meter state match the decoded file instead of leaking the previous file's channel layout
- Given 已满足以下前置条件：a supported compressed format such as OGG or WMA
- When 执行以下操作：AudioEngine decodes and loads the file
- Then 应得到以下结果：the file becomes auditionable through the same load path without requiring a separate fallback workflow
- Given 已满足以下前置条件：the user exports a single channel or processed file with mono, solo, reverse, or pitch enabled
- When 执行以下操作：export runs
- Then 应得到以下结果：only the requested output is transformed, and the source file on disk is not overwritten in place

### Failure / Recovery

- Given 已满足以下前置条件：decode fails or the file is unsupported/corrupted
- When 执行以下操作：AudioEngine attempts to load it
- Then 应得到以下结果：the engine reports a failure and does not masquerade as successfully loaded with stale playback state
- Given 已满足以下前置条件：output device reinitialization fails
- When 执行以下操作：the user switches output devices
- Then 应得到以下结果：AudioEngine fails safely and preserves a usable playback state or a clean stopped state instead of deadlocking the engine

### Non-Functional

- Given 已满足以下前置条件：playback is active
- When 执行以下操作：DSP or device changes are applied
- Then 应得到以下结果：control changes commit without a long blocking synchronous path on the UI thread, and export/render loops remain bounded to chunked processing rather than a single uninterruptible monolith
- Given 已满足以下前置条件：Magic EQ is processing the current file
- When 执行以下操作：the user rapidly changes the center frequency, band width, Pass/Notch mode, or bypass state
- Then 应得到以下结果：the active filter state is not overwritten by new coefficients; the new filter state is rendered in parallel and crossfaded over about 10 ms, with the latest pending selection applied after the current transition and no added signal delay
- Given 已满足以下前置条件：loudness and waveform detail are computed during decode
- When 执行以下操作：AudioEngine finishes analysis for the current file
- Then 应得到以下结果：LUFS, true peak, and RMS values describe the decoded current file rather than a previous request token

## Interface Binding

| Interface / Entry | Kind | Source | Expected behavior |
| --- | --- | --- | --- |
| `AudioEngine::loadFileAsync()` | worker | Graphify | Owns async decode/load for the current file |
| `AudioEngine::switchOutputDevice()` | API | Graphify | Reinitializes output device without leaving the engine in a stale state |
| `AudioEngine::setMono()` / `setChannelSolo()` | API | source scan | Applies channel-routing semantics consistently across playback and processed export |
| `AudioEngine::setPitch()` / `setFreqShift()` / `setReverse()` | API | source scan | Applies DSP state to current audition/export behavior |
| `AudioEngine::setCenterSense()` | API | source scan | Applies stereo-field processing without becoming a hidden global state |
| `AudioEngine::playRegion()` / `exportRegion()` | API | tests | Preserves selected region semantics for audition/export |
| `AudioEngine::exportFileProcessed()` / `exportSingleChannel()` | API | tests | Produces transformed output copies without mutating the source asset |
| Audio diagnostics | diagnostics | QA protocol | Exposes load/export/device-switch runtime evidence |

## Graphify Binding

**Source graph:** `graphify-out/main/interface-call-graph.json` and `graphify-out/main/audit-call-graph.json`
**Impact target(s):** `AudioEngine::loadFileAsync`, `switchOutputDevice`, `setMono`, `setChannelSolo`, `setPitch`, `setFreqShift`, `setReverse`, `setCenterSense`, `playRegion`, `exportRegion`, `exportFileProcessed`, `exportSingleChannel`
**Audit classification requirement:** AudioEngine unknowns must be reviewed before completion because stale playback, wrong channel routing, or destructive export semantics are user-visible and high risk.
**Dynamic calls:** review playback callbacks, device-switch callbacks, and export/render loops.
**Unknown calls:** review AudioEngine unknowns in `UNKNOWN_CALLS.md`, especially load/export/device regions.

## TDD / QA Proof

| Proof ID | Kind | Artifact / command / event | Pass signal | Status |
| --- | --- | --- | --- | --- |
| TEST-AUDIO-OGG | integration | `TestOggPlaybackSupport` | OGG decode/load path passes | active |
| TEST-AUDIO-WMA | integration | `TestWmaDecodeSupport` | WMA decode/load path passes | active |
| TEST-AUDIO-CENTER-SENSE | unit | `TestCenterSense` | center-sense DSP behavior passes | active |
| TEST-AUDIO-TIME-STRETCH | unit | `TestTimeStretch`, `TestTimeStretch2` | archived processor unit behavior only; not linked into HarkenSearch | retired |
| TEST-AUDIO-CHANNEL-ROUTING | integration | `TestChannelRoutingSettings` | mono/solo routing settings pass | active |
| TEST-AUDIO-METER | unit | `TestMultichannelMeter` | channel-meter vector resizes and updates correctly | active |
| TEST-AUDIO-PROCESSED-EXPORT | integration | `TestProcessedFileExport` | processed export behavior passes | active |
| TEST-AUDIO-SINGLE-CHANNEL | integration | `TestSingleChannelExport` | single-channel export behavior passes | active |
| TEST-AUDIO-SILENCE-SKIP | integration | `TestSilenceSkipGate` | playback gate behavior stays bounded during load/seek transitions | active |
| TEST-MAGIC-EQ-TRANSITION | unit | `TestMagicEQDsp` | narrow and wide frequency jumps, continuous center/width movement, mode switches and bypass transitions stay finite and within measured peak/adjacent-sample limits across 44.1/48/96 kHz and 32/64/256/1024-frame blocks | active |
| DIAG-AUDIOENGINE | diagnostics | `Audio.loadFile`, `SwitchOutputDevice*`, `ExportFileProcessed*`, `ExportRegion*` runtime events | load/export/device lifecycle events observed without stale-state errors | active |

## Missing Coverage

- Missing proof: end-to-end output-device hot switch under real hardware is still not asserted in automation.
- Missing proof: LUFS-compliance workflow acceptance is not yet covered by a dedicated AudioEngine behavior test.
- Residual gap: audible artifact quality for extreme DSP combinations is still human-verified.
- Human-only verification: real device-switch ergonomics and perceived audio quality.

## Change Gate

- [x] Refresh Graphify main and audit graphs with `python tools/graphify_auto_refresh.py --once`.
- [x] Re-run AudioEngine contract tests.
- [x] Review AudioEngine `UNKNOWN_BLOCKING` entries.
- [x] Update this contract if DSP, channel, decode, device, or processed-export semantics change.

## Decision Log

| Date | Decision | Reason | Decider |
| --- | --- | --- | --- |
| 2026-06-04 | Split out an AudioEngine-specific contract instead of continuing to scatter engine guarantees across playback-lifecycle, waveform, and delivery modules. | Review found core engine promises like DSP, channel routing, decode support, and processed export had no dedicated behavior owner. | Engineering |
| 2026-06-04 | Kept DB-switch cleanup, waveform publish policy, and downstream DAW behavior outside this contract. | Those behaviors already belong to neighboring contracts; duplicating them here would blur module boundaries again. | Engineering |

## Magic EQ 体验切片（RQ-20261014-01）

### 固定标称边缘斜率与局部测量

- 32 px 正式传输栏的斜率可见区与点击区共用右边界，并止于频率轨道前；点击对数刻度不打开档位菜单。斜率全文放得下才画完整单位，否则显示带省略符的数值或仅模式曲线，悬停显示两侧完整值与区间；不会在中间裁断文字。
- 频段文字位于轨道上排，按整条轨道实际宽度显示范围、中心频率或信息标记；完整范围仍可悬停读取。与之相交的刻度数字让位，轨道与两侧抓手保持可见。状态文字按 ImGui 实际字宽选择当前语言的紧凑同义词，放不下时改为无文字的模式曲线或旁通横线，仍裁在状态格内。三档曲线中的极陡档用独立尖直折线，不借柔和曲线放大高度冒充。

- Pass 与 Notch 的主控为固定**标称边缘斜率** 12/24/48 dB/oct，另有独立的“砖墙近似”。两种模式图标分别上拱、下凹，不表示 Pass 提升增益。数值来自 Butterworth 低通原型阶数 N=2/4/8 的裙边定义约 6.0206N dB/oct；砖墙近似 N=24、标称约 144 dB/oct，不等于数学砖墙、全域恒定局部斜率或零泄漏。带通/带阻变换后总阶数 2N，实际二阶节数 N，不能把 2N 再乘 6.0206 当作标称值。Notch 采用本产品自己的同等级名称，不冒充通行的 Notch 全域固定斜率标准。
- 详情在当前选区、模式、档位和采样率下显示中心、两侧 -3 dB 频点、由中心与频段宽度算出的 Q，以及两侧实际局部 ΔdB/Δlog2(f) 与测量 Hz 区间。局部数值仅在悬停详情求值；主控不因频点变化而跳数。界面只在完整参数键变化时刷新四档尾长可用性，有界四槽缓存；音频回调不做显示分析或长仿真。
- 接近 20 Hz、Nyquist、选区过窄、采样率不可用或旁通时，局部详情对受影响的一侧显示短横与当前语言原因，不伪造数字。32 px 传输栏空间不足时只缩短标签或只画模式曲线，不压住轨道、抓手、状态。三语账本提供当前语言的主控标题、近砖墙、禁用原因与详情文案；正式主控不再以柔和/陡峭/极陡或 SOFT/STEEP/VERY STEEP 命名。
- 可用性按当前正式滤波器极点估算峰值参考 -80 dB 衰减至 0.2 秒的边界，频率超当前采样率可处理范围或估算尾长超限的档位在菜单置灰并说明。拖动不能进入无效组合，换文件后若原组合失效则显示旁通；DAW 文件渲染在创建输出前以冻结参数与源采样率复核，同样拒绝无效组合，不回退原声。0.2 秒是待完整网格与人工听感进一步验证的初始上限，极点估算与脉冲实测须并列记录，不能把估算当全部实测。

- Magic EQ 用于当前文件实时试听；Pass 保留选区频段，Notch 抑制选区频段，关闭时旁通。用户把当前试听文件拖到 DAW 时，传输链在隔离临时副本里冻结 Magic EQ 与已有处理参数并渲染；Pass/Notch 写入交付副本，旁通不应用 Magic EQ。渲染成功后才交付该副本路径，失败不得退回原声；源文件不修改。该能力不改变独立导出菜单的现有行为。
- 选区以 20 Hz 至 20 kHz 对数频率轴上的中心点与半宽保存。拖动左端点只移动低频边界并固定高频边界；拖动右端点只移动高频边界并固定低频边界。中心拖动平移选区，Ctrl/Cmd 垂直拖动调整宽度，滚轮调整宽度。
- 32 px 原生控件使用传输栏主题底色和主题状态色；最外侧两个抓手只调控件宽度。Pass、Notch、旁通同时显示不同颜色与本地化文字。中、英、日文字由 Localization 的 UTF-8 文案入口提供。
- 频谱选区覆盖层从同一组低、高频边界映射到当前文件的有效频谱图，只影响绘制，不改变 DSP 处理范围。仅在频谱缓存仍对应当前文件时显示；seek 会清空 Magic EQ 滤波状态，换文件会建立新的数据源状态。

### 2026-10-20 连续播放器栏 A：模块详细设计

- **设置与可用域**：新安装且未保存频宽时，默认半宽为对数频率轨道的 20%，即完整选区约 40%；已有保存值优先。重置也回到该目标。首次启用若当前采样率与档位的 200 ms 尾长门禁拒绝目标宽度，控件向更窄频段寻找最近可用值；若没有可用值，仍维持关闭并用现有本地化原因说明。普通拖动继续按原门禁回退，不修改滤波器、保存结构或门禁数值。此模块可用 DSP 尾长测试与设置读回分别验证。
- **传输栏绘制**：EQ 控件继续占 32 px 高、沿用 46×24 星形开关、原左右宽度抓手、模式格和整条 20 Hz–20 kHz 对数轨道。去除控件整块独立深色外框，共享播放器栏底色。斜率、频段和刻度按各自区域的实际字宽排布，频段上排优先；模式短词仍放不下时改用无文字图形，完整值保留在悬停详情。点击区域和处理参数映射不变。此模块用正式源码布局契约和真实 EXE 截图分别验证，源码检查不能替代视觉结论。
- **频谱叠层**：只对当前文件的有效频谱图绘制中性暗幕。Pass 只覆盖选区外上下两段，Notch 只覆盖选区内，旁通在绘制暗幕前返回；两条边缘继续用模式主题色，不对声音增益或处理频段做视觉反推。此模块用方向/绘制顺序契约、隔离状态回读和真实 EXE 频谱画面分别验证。
- **限制反馈**：砖墙近似拖动触发尾长门禁时，保持最近已接受频段，气泡显示受限频点，悬停显示对应的 0.2 秒原因与恢复建议；无效参数或换文件后的失效状态仍走原有可见旁通。此模块用尾长可用性测试与正式控件实际拖动分别验证。
- 标准 decoder 的单声道播放也会进入 Magic EQ 试听链，TimeStretch 缓存按实际解码声道数初始化，避免把单声道帧按双声道读写；音频回调通过无锁原子快照读取 UI 设置，遇到 UI 正在发布设置时保留上一帧的完整快照，不在回调里等待锁。
- Magic EQ 滤波与双滤波器过渡后的输出逐帧检查样本峰值；任一声道超过 0.999 时，对同帧所有声道施加同一向下增益，随后按 20 ms 时间常数恢复。未发生过载的流始终为精确单位增益；无预读、无新增算法延迟，保护启动可能改变瞬态，不承诺 true-peak。旁通输出保持原声，seek/换文件清空增益状态。现有导出路径另有非 EQ 的过满幅 soft-clip；Magic EQ 自身正常电平不走该处理。
- AudioEngine 收到非有限数、超控件范围或未知模式/档位时，将当前 Magic EQ 标为旁通，保留可见的无效参数状态供正式控件提示；音频快照同时发布关闭状态，不继续使用此前开启的滤波参数。恢复有效设置后才可重新开启。
- DAW 交付与试听使用同一组 Magic EQ 处理参数和顺序；整文件、当前文件选区、声道、采样率、已有效果顺序沿用现有拖出契约。参数快照在本次拖出准备时固定，用户之后的控件变化不影响正在生成的副本。渲染失败时清楚报错且不发送半成品或原声。
- 性能复测入口：Release 的 TestMagicEQDsp 设置 ARIS_MAGIC_EQ_BENCH=1 后，用隔离合成双声道音频测旁通、Pass/Notch 四档和连续参数更新，覆盖 44.1/48/96 kHz、32/64/256/1024 帧，预热后给出每块时间的 p50/p95/p99/max 与预算。另以多轮有界参数更新、活动与静置读取私有提交、工作集、存活堆块/字节、句柄/GDI/USER。它只证明 DSP/斜率计算层 CPU 与内存趋势，不创建原生窗口、不测频谱 GPU/整窗帧时间或真实鼠标拖动。

| TEST-MAGIC-EQ-DSP | unit | `TestMagicEQDsp` | 对数频率映射、左右边界钉住、Pass/Notch/旁通、参数快照 | active |
| TEST-MAGIC-EQ-UI | source contract | `TestMagicEQUiContract` | 三语 UTF-8、32 px 控件、端点/宽度抓手、状态色与当前频谱高亮 | active |
| TEST-MAGIC-EQ-SCOPE | source contract | `test_magic_eq_scope_contract.py` | 独立导出与处理状态汇总不读取 Magic EQ；仅正式 DAW 拖出路径可选择应用 | active |
| TEST-MAGIC-EQ-DAW | source + rendered fixture | `test_magic_eq_daw_delivery_contract.py` + processed-file fixture | 正式拖出链冻结参数、仅成功后交付；Pass/Notch/旁通输出正确且保留格式/范围/原文件 | active |

## 第6批倒放失败恢复

本批只补倒放做不成时尽可能装回当前原声。不新增倒放解码能力，不接插件链，不证明真实播放。

### 输入与成败

- 输入：setReverse(bool) 请求值、当前加载路径、是否已有可播声音、点击前 wasReversed/wantedReversed。
- 成功：return true且实际 reverse 等于请求值、有 sound；工作流和快捷键才镜像选区与播放头。
- 失败：reload 仍 return !restoreForwardAfterReverseFailure；能装则 installForwardSound 装回正向原声，seek 用 originalPos；正向重装失败仍为 false，不伪成功。

### 五项所有权

- identity：当前加载路径、请求 reverse 值、sound 是否存在、点击前 wasReversed/wantedReversed。不用选中行或插件链做身份。
- generation：一次同步 setReverse；成功返 true 才允许镜像。失败发布 m_reverse=false，能装则正向原声与原 position。不新增计数器。
- cancel：无 worker/token。下一次点击是下一次同步调用。任一失败不遗留半次镜像。
- stale-result：无 future/ports/跨帧 this。installForwardSound 仅在 reload 栈内；提示只读本帧 snapshot。不接 AuditionPluginChain。
- 生命周期：保持 745 unique_lock；loadReversedAudio 期间 unlock/relock。失败恢复持锁；sound/decoder/pcm 由 reload 管理。仍有声但本次操作失败必须 false。

### 前端 bool 与快照

- workflow/adapter 转发 setReverse 的 bool；开启或关闭失败都不镜像。
- 快捷键仅在 reverseApplied 为 true 时镜像。
- 底栏回读 captureSnapshot(reverseAfter)，块内禁 state.audioEngine。复用 L(Player_ReverseFailed)，不加键，后台不调 L()。

### 回滚与证据级别

- 回滚仅撤本批函数、签名、测试追加、单处 cmake 与本附录。
- FakeEffects 证明工作流门与选区；源码合同证明恢复控制流；完整 BUILD 证明产品编译。不证明引擎 IO/听感／GUI/E2E。
## QA-only 长波形/电平（第10批）

AudioEngine 增加 generateWaveformFromOpenDecoder。ForFile 打开 decoder 后把已知和未知长度都交给 helper，并由 ForFile 关闭 decoder。未知长度按 8192 帧一块出峰值。collectPeaks=false 时不写入 peaks/rms 数组。DecodeFileToInterleavedF32 增加 maxFrames，超预算失败并清空缓冲。AsyncWorker 电平调用 collectPeaks=false。后台不调用 L()。无新文案键。

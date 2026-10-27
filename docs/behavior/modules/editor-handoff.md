# Editor Handoff Behavior Contract

## Header

**ID:** BDD-EDITOR-HANDOFF
**Module:** Editor Handoff
**Status:** Active
**Priority:** P1
**Owner:** Aris
**Created:** 2026-06-12
**Last Updated:** 2026-07-11

## Product Behavior

**User promise:** Selected sounds expose one truthful editor-handoff surface. Aris shows only capabilities backed by a reviewed public interface or by an Aris-owned adapter, and it never labels an unavailable path as a successful direct insert.
**Primary workflow:** The user selects sounds, opens Send to Editor, chooses REAPER, Pro Tools, Cubase, or Nuendo, reads the current preflight state, chooses an available handoff action, and receives a concrete success or recovery message.
**Non-goals:** This contract does not ship undocumented vendor protocols, private binary-analysis artifacts, vendor runtimes without redistribution permission, or a claim that every host supports direct timeline insertion.

## 中文语义

**用户承诺：** 音效师在“发送到编辑器”里看到的能力必须真实；能直送才显示可直送，不能直送时明确给出配置、导出或拖拽恢复路径，源音频不会被原地修改。
**核心流程：** 用户选择素材，打开发送菜单，选择目标宿主，查看预检结果，再执行当前被证明可用的发送方式；完成后显示目标、方式和结果。
**非目标：** 本契约不引入未公开的厂商协议，不携带来源不清的私有研究实现，也不替用户接受 Avid、Steinberg 或其他厂商的授权条款。

## BDD Scenarios

### Happy Path

- Given 用户已选择至少一个有效音频文件且目标宿主适配器通过预检
- When 用户执行“发送到编辑器”并选择可用的宿主发送方式
- Then Aris 应通过已审阅的适配器发送文件并显示可核验的成功结果

### Edge Cases

- Given 用户选择了多个文件、选区渲染或带试听处理的素材
- When 用户切换不同宿主或发送方式
- Then Aris 应按照目标能力生成对应副本或原始路径列表且不修改源文件

- Given 当前试听文件启用了 Magic EQ，或同时启用了现有其他处理
- When 用户把该文件拖到 DAW
- Then Aris 应用拖出开始时冻结的参数生成临时副本；启用时烘焙当前 Pass/Notch 频段，旁通时保持现有效果顺序且不应用 Magic EQ，只有渲染成功后才投递副本路径

### Failure / Recovery

- Given 用户选择的宿主未运行、适配器缺失或授权运行库未配置
- When 用户尝试发送
- Then Aris 应阻止虚假成功并显示配置、启动宿主、导出或拖拽等明确恢复路径

- Given Magic EQ 拖出副本渲染失败
- When 用户尝试把文件拖到 DAW
- Then Aris 应显示失败原因，不发送半成品，也不静默退回源文件路径

### Non-Functional

- Given 用户重复打开发送菜单或连续发送多批素材
- When Aris 刷新能力、预检和最近发送记录
- Then 界面应保持可响应且诊断信息不得泄露本机绝对路径或私有研究数据

## Interface Binding

| Interface / Entry | Kind | Source | Expected behavior |
| --- | --- | --- | --- |
| `SendToDAW` | UI | product + source | Routes selected files only through a truthful target-specific path |
| `GetDawCapability` | API | product + source | Returns capability labels that match the currently reachable backend |
| `Aura::CubaseAdapter::EvaluatePreflight` | API | Aris-owned adapter contract | Keeps Cubase/Nuendo direct-insert claims disabled until the public adapter is genuinely ready |
| `Aura::ProTools::ProbeInstalledCapability` | API | official/user-supplied SDK boundary | Requires an installed and licensed PTSL environment before native handoff |
| `OpenWwiseTransferPanelForRecords` | UI | official WAAPI contract | Opens the separate Wwise transfer surface without mixing it into DAW capability claims |
| editor-handoff diagnostics | diagnostics | source + QA | Reports target, readiness, dispatch, result, and recovery reason without private evidence |

## Graphify Binding

**Source graph:** `graphify-out/main/interface-call-graph.json` and `graphify-out/main/audit-call-graph.json`
**Impact target(s):** `SendToDAW`, `GetDawCapability`, Cubase/Nuendo adapter preflight, Pro Tools capability probe, Wwise transfer entry
**Audit classification requirement:** Any `UNKNOWN_BLOCKING` node that can turn an unavailable host route into an apparent success must be resolved before release.
**Dynamic calls:** Review target-specific callbacks, named-pipe/WAAPI calls, and user-supplied SDK loading.
**Unknown calls:** Review editor-handoff `UNKNOWN_BLOCKING` entries before completion.

## TDD / QA Proof

| Proof ID | Kind | Artifact / command / event | Pass signal | Status |
| --- | --- | --- | --- | --- |
| TEST-HANDOFF-SURFACE | contract | `TestSendToTargetUiContract` | public target surface and recovery routes remain truthful | active |
| TEST-HANDOFF-MODEL | unit | `TestSendToTargetSurfaceModel` | capability and preflight presentation is deterministic | active |
| TEST-STEINBERG-PUBLIC | unit | `TestCubaseDirectInsertContract` | Aris-owned bridge contract rejects missing/unready adapters | active |
| TEST-PROTOOLS-BOUNDARY | contract | `TestProToolsToolsContract` | PTSL runtime remains user-supplied and license-gated | active |
| DIAG-HANDOFF | diagnostics | editor-handoff dispatch/result events | target, result, and recovery reason are observable | active |

## Missing Coverage

- Missing proof: a release-safe end-to-end smoke against each supported host still requires the relevant host application and licensed SDK/runtime on a QA workstation.
- Suspected stale proof: historical private Nuendo integration notes are not proof and are removed in Stage 1.
- Human-only verification: final host workflow ergonomics and audio-editor placement behavior.
- Product, audio-workflow, or license decisions that cannot be resolved technically must be recorded for human review rather than hidden behind a success state.

## Change Gate

- [x] Public behavior contract is explicit before Stage 1 implementation.
- [x] Run `TestSendToTargetUiContract`, `TestSendToTargetSurfaceModel`, `TestCubaseDirectInsertContract`, and `TestProToolsToolsContract`.
- [x] Run the Stage 1 provenance gate and public release compliance checker.
- [x] Review module-relevant `UNKNOWN_BLOCKING` entries through the full behavior/audit gate.
- [ ] Perform host-specific human smoke only when the licensed host/runtime is available.

## Public Steinberg Adapter Truth Contract

- Given Cubase or Nuendo is selected
- When the public adapter is missing, the host is not running, its endpoint is offline, or its helper cannot be resolved
- Then Host Handoff remains disabled and the precise preflight reason is shown.
- Given attach succeeds but the host project or selected track is not insert-ready
- When the user attempts a handoff
- Then Aris reports `Host not ready` and does not equate endpoint liveness with insert readiness.
- Given a host snapshot is ready
- When Aris constructs the final insert request
- Then project, track, cursor, session, and snapshot identity remain explicit so drift cannot become a false success.
- Given the public adapter returns a structured result
- When the handoff completes
- Then recent history and the send result show the real success, partial/remapped result, or typed recovery state.

TDD proof:

- `TestCubaseDirectInsertContract` locks helper discovery, attach readiness, snapshot identity, structured addressing outcomes, and fail-closed behavior.
- `TestSendToTargetSurfaceModel` and `TestSendToTargetUiContract` lock the visible capability and recovery wording.

## Wwise Transfer Exposure Contract

- Given Wwise transfer is exposed but the current edition does not enable `wwise_transfer`
- When Aris builds the handoff surface
- Then Wwise remains visible but disabled with an honest reason.
- Given `wwise_transfer` is enabled
- When the user opens Wwise transfer from selected records
- Then Aris opens the dedicated transfer panel without claiming Wwise-side import success before WAAPI returns it.
- Given live Wwise object verification is unavailable in automated CI
- When release evidence is assessed
- Then real project mutation remains a licensed-host QA check rather than an inferred pass.

TDD proof:

- `TestResultActionMenuModel` and `TestSendToTargetSurfaceModel` lock exposure and capability truth.
- The Stage 1 provenance gate pins request construction to Audiokinetic's published WAAPI fields.

## Pro Tools Licensed-Runtime Contract

- Given the user selects Pro Tools
- When the user-supplied bridge runtime is missing, Pro Tools is not running, or no live session is ready
- Then the action remains disabled with a concrete setup or host-readiness reason.
- Given a separately licensed runtime and live session are ready
- When the user opens the Pro Tools target
- Then Aris may expose the existing timeline-spot action without bundling the vendor runtime by default.
- Given selected-track placement cannot be proven
- When the helper evaluates the request
- Then it fails closed and must not create a new track as an undocumented substitute.
- Given a helper timeout, host reset, session change, moved edit cursor, or non-ASCII clip name
- When dispatch runs
- Then the helper preserves typed failure/recovery semantics and the original clip identity.

TDD proof:

- `TestProToolsToolsContract` and the Pro Tools bridge tests lock setup, runtime ownership, timeout, session, cursor, and encoding boundaries.
- Release configuration keeps `AURO_BUNDLE_AVID_PTSL=OFF`; live host validation remains conditional on a separately licensed QA environment.

## Decision Log

| Date | Decision | Reason | Decider |
| --- | --- | --- | --- |
| 2026-07-11 | Remove the private Nuendo research implementation and retain only the public Aris-owned adapter/fallback surface. | The private surface was disabled in the public baseline and had unverifiable provenance; removal preserves public behavior while eliminating risk. | Technical Release Manager (delegated) |
| 2026-07-11 | Ground Wwise transfer calls only in Audiokinetic's published WAAPI contract. | Public API documentation is sufficient for the current import, property-copy, and undo-group behavior. | Technical Release Manager (delegated) |
| 2026-07-11 | Keep vendor SDK/runtime acceptance outside Aris automation. | Users or release owners must accept vendor terms; Aris may only detect a separately installed runtime. | Technical Release Manager (delegated) |

# 试听链底层依赖模块详细设计（第5批）

本批只把已交付的同步领域模型 AuditionPluginChain 编进 745 旧布局隔离预览，不接现有播放、不加界面或可听插件。不是整卡完成，也不是停放插件链产品卡的解停。

## 承诺

- 内存中按 steps_ 顺序处理一份 input 副本。
- Unknown 预检失败或某步 handler 返回 false 时：ok=false，output 回到原 input，appliedSteps 为空。
- 全部步骤成功才发布 working 与 appliedSteps，ok=true。
- 保留 aura::audition 兼容 API 与标识，不为品牌改名改符号。
- 不改变现有播放按钮、AudioEngine、前端、VST、设置。

## 输入、输出与步骤

- 输入：Run(const std::vector<float>& input) 的按值副本；Append 追加的 PluginStepKind 顺序；可选 SetStepHandler。
- 步骤种类：Passthrough、Reverse、Normalize、Unknown。
- 输出：AuditionPluginChainResult{ok, output, appliedSteps}。
- 默认 handler 是无捕获自由函数：倒序、按峰值归一、直通。

## Unknown 预检、成功发布、失败回原样

- 先扫一遍 steps_。任一 Unknown 立即失败返回，不执行任何 handler。
- 预检通过后再逐步调用 handler。某步没有 handler 或返回 false：output=原 input，appliedSteps 清空，ok=false。
- 全部成功：output=working，appliedSteps=已成功步骤，ok=true。
- 失败不把半成品当成功，也不把失败当成功。

## 同步所有权五项

- identity：一次 Run 的 input 副本、steps_ 顺序、可选 handler。无库、文件、设备、surface。
- generation：无计数器。只有 ok=true 才发布 working/appliedSteps。
- cancel：同步 const Run。Unknown 预检或 handler false 即中止。无 worker、无 token。
- stale-result：按值返回。无 future、ports、跨帧 this。handler 不捕获 AppState/this/引擎缓冲。
- 生命周期：链对象拥有 steps_ 与三个 std::function。默认 handler 无捕获。测试 lambda 只在 main 栈里计数和合成 float 数组。不改 AudioEngine 寿命。

## 两源测试闭包

- TestAuditionPluginChain 只链 tests/test_audition_plugin_chain.cpp 与 src/AuditionPluginChain.cpp。
- include 当前预览 source 与 source/src。MSVC /W3 /MP /utf-8。EXCLUDE_FROM_ALL。
- 禁止 Utils、layout、migrator、Database、AudioEngine、隔离 FI 头、生产路径。

## 测试场景与证据边界

- empty chain：成功，output 等于 input，appliedSteps 空。
- reverse then normalize：成功，两步按序记录。
- second-step failure rolls output back：失败，output 回原 input，appliedSteps 空，前后 handler 都尝试过。
- unknown rejects：失败，output 回原 input，appliedSteps 空，前后 handler 计数为 0。
- 纯模型测试不证明播放、听感、插件 UI、合法媒体、真设备。

## 回滚

- 只撤三个新源文件、本设计、CMakeLists 一行、behavior cmake 一段、台账第 4/5 批两行。
- 不删旧证据、不回退已审前批、不改播放或确认门。

## 明确不接播放

- 本批编入产品但不接线。main、AudioEngine、AuditionFrontend、Transport、miniaudio、VST、UI 零引用本模型。
- 不服务现有播放按钮，不增加可听插件，不解停 RQ-20260715-03。

## 关联位置

- 产品卡：Cuevi RQ-20261010-NONDOCK-PREVIEW。
- 没有单独计划：复用总合同与本批附属合同。
- 行为说明：本文件。
- 测试：tests/test_audition_plugin_chain.cpp。

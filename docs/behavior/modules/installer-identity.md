# 安装器身份校验（第11批）

本模块把冻结已交付的安装身份与旧卸载校验搬回 745 旧布局。新产品使用 Harken AppId 31B6E215。旧 Aris GUID 只用来查找卸载项。旧卸载只在真正开始安装时发生，只接受安装目录下的 unins000.exe，只执行已校验的临时副本。本批不给这台电脑真安装或卸载。

## 输入与输出

- 输入：Inno 安装步骤、旧卸载注册表项、安装位置、可选的验签工具路径与期望叶子主体。
- 输出：新 AppId 的安装身份；或者在 ssInstall 卸旧成功后继续；或者已尝试卸旧失败时 Abort。
- 不输出：向导阶段的卸旧、原 HKCU 路径直接执行、用户 profile 安装路径上的卸载器。

## 主要函数

- CurStepChanged：仅 ssInstall 调用 UninstallLegacyArisIfPresent，失败 Abort。
- InitializeSetup：保持 WebView2 询问，不卸旧。没有 PrepareToInstall。
- IsTrustedLegacyUninstaller：安装位置 + unins000.exe + 非 profile；有工具时先复制再校叶子主体，只记 TrustedLegacyStagedExe。
- ExecLegacyUninstaller：校验通过才 Attempted，只 Exec 副本。
- SigntoolLeafSubject：只认 Signing Certificate Chain 后第一个 Issued to。
- UninstallLegacyArisIfPresent：运行中则拒绝；已尝试失败则停装；未尝试不阻断升级。

## 五项所有权

1. identity：新 AppId 是产品安装身份；旧卸载身份由规范安装位置、unins000.exe、叶子签名主体和校验副本组成。测试假负载不是用户安装身份。
2. generation：一次同步安装步骤；ssInstall 当次创建临时快照。HKCU 字符串不是执行身份。
3. cancel：用户没到 ssInstall 就取消不卸旧。InitializeSetup 不能卸。没有新线程池。
4. stale-result：不执行校验之后可能变化的原路径，只执行 TrustedLegacyStagedExe。缺工具或校验失败不尝试执行；已尝试失败才 Abort。
5. 生命周期：Inno 同步控制；临时工具与副本按原脚本清理。AudioEngine/Database/profile 不参与。

## 取消时机

向导取消发生在 ssInstall 之前，不会半卸。PrepareToInstall 不存在。

## 安全探针

两路 ISCC 只编译受控假负载：plain 与 SignToolPath+TrustedSignerSubject。脚本、资源、TEMP 与输出都在 D:/Aris-QA/test-databases/RQ-20261010-NONDOCK-PREVIEW-installer-identity 的本次子目录。不执行编出的安装程序，不调用真实验签工具。缺 ISCC 记 SKIP，不是编译绿。

## 回退

撤掉 iss 身份/卸旧块、lifecycle 追加断言和 iss_compiles 探针即回到本批基线。不回退第0–10批，不删他人文件。

## 证据局限

已证：源码合同、受控假负载两路 Inno 编译、完整产品编译与安全回归。未证：真安装/卸载/升级、注册表、证书主体真实可信、GUI/人眼/E2E。


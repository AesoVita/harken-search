# Harken Search

Professional sound manager.

Windows desktop application for audio library browsing, search, preview, metadata workflows, and DAW bridge utilities.

## 仓库去向

- 源码、问题反馈和后续开发：https://github.com/xindong/harken-search
- 公开仓从经过整理的源码开始，旧私有仓的历史记录单独保留。
- `main` 用于正式构建，`qa/current` 用于已验收改动的集成与 QA 包。

## 提交边界 / Contribution boundary

- 只提交当前任务验收所必需的最小改动：代码、测试、构建配置、以及必要文档。
- 与当前任务无关的迁移资料、历史卡片、本地流程状态、缓存、个人配置、AI/agent 运行产物不进入 PR。
- 共享规则以 [CONTRIBUTING.md](CONTRIBUTING.md) 为准；不确定某个文件该不该提交时，默认先不要提交。

## Build

Windows quick build / packaging guide:

- [BUILD_WINDOWS.md](BUILD_WINDOWS.md)

## Release compliance

Follow [BUILD_WINDOWS.md](BUILD_WINDOWS.md) for the ordinary Debug build or the
public release build with its required package key ID and verification public key.
Official packages are built from `main` by the `package-update.yml` workflow.
The signing private key is never part of the source tree or a CMake command.
Run the compliance checks before building and after assembling the release:

```powershell
py tools/check_release_compliance.py
py tools/check_release_compliance.py --release-dir build_public/Release
```

## Public-release notes

- Build public packages with `-DARIS_PUBLIC_RELEASE=ON`.
- Keep `-DAURO_BUNDLE_AVID_PTSL=OFF` by default unless Avid redistribution has been manually reviewed and approved for that package.
- The retired React/WebUI prototype is not part of this repository or release. Harken Search keeps its compiled native UI and the local WebView2 welcome surface implemented in `src/WindowsMain.cpp`.
- Release-facing AI/privacy expectations are documented in [PRIVACY.md](PRIVACY.md) and [AI_FEATURE_DISCLOSURE.md](AI_FEATURE_DISCLOSURE.md).
- 法律文件：[用户协议](docs/legal/EULA.md)、[隐私政策](docs/legal/PRIVACY_POLICY.md)、[开源发布总说明](docs/legal/OPEN_SOURCE_RELEASE_NOTICE.md)。
- English legal documents: [License Agreement](docs/legal/EULA.en.md), [Privacy Policy](docs/legal/PRIVACY_POLICY.en.md), [Open-Source Release Notice](docs/legal/OPEN_SOURCE_RELEASE_NOTICE.en.md).
- “Harken Search”, “Harken”, “XD”, and related logos are trademarks of XD Network Inc. The Apache License 2.0 does not grant trademark rights.

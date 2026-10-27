# Contributing to Harken Search

Harken Search uses short-lived branches and reviewed pull requests. Follow this
file, [MAINTAINERS.md](MAINTAINERS.md), [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md),
and the pull request template for the public workflow and release boundary.

Report security issues through [.github/SECURITY.md](.github/SECURITY.md), the
single authoritative reporting policy for this repository. Do not disclose
vulnerabilities in public issues, discussions, or pull requests.

## Before changing code

1. Describe the problem, user impact, scope, non-goals, risks, and acceptance
   evidence.
2. Select a primary module domain from [MAINTAINERS.md](MAINTAINERS.md) and
   identify any secondary reviewers.
3. Create a topic branch from the current default branch. Do not push directly to `main`.
4. Add the smallest automated regression proof before changing behavior.

Every change is reviewed independently before merge. Do not merge a topic branch
directly to `main`.

By contributing, you agree that your contribution may be licensed under the
repository [LICENSE](LICENSE). Every commit must be signed off with
`git commit -s`, adding a `Signed-off-by: Name <email>` line.
This [Developer Certificate of Origin (DCO)](https://developercertificate.org/)
sign-off applies to every commit.

## Commit and pull request titles

Title the result. If users see a change, say what Harken Search can do now or
what was fixed. For docs, process, or no-behavior-change refactors, say what
rule or cleanup this change lands; do not invent a product capability. Do not
write frontend/backend. Do not stack module lists.

`type(scope): plain summary` is a habit, not a form; a readable result is
enough. Empty or shell titles are a human review matter, not a repository
gate. A readable title may also name the visible result directly, for example
`feat(ui): 启动页改用官方 Logo`.

Use Search scopes, not Studio's `studio` / `runtime` / `hub` / `pack` / `sdk`:

| Scope | Use for |
| --- | --- |
| `search` | Query, filters, and hits |
| `library` | Sound library and database |
| `scan` | Scanning and ingest |
| `audio` | Playback, waveform, and decode |
| `ui` | Visible interface and interaction |
| `update` | Check for updates and auto-update |
| `installer` | Install and uninstall |
| `i18n` | zh / en / ja copy |
| `docs` | Documents and process |
| `ci` | Gates and build |

标题写结果：用户看得见就写软件多了什么，或修了什么现象；文档和流程写改了哪条规则。不要写前端/后端，不要堆模块名。`类型(范围): 人话` 只是习惯，不是填空。范围只用上表。每个 commit 使用 `git commit -s`。

## Build and test

From a Visual Studio developer environment on Windows:

```powershell
cmake -S . -B build_dbg -G "Visual Studio 17 2022" -A x64
cmake --build build_dbg --config Debug --target HarkenSearch -- /m
ctest --test-dir build_dbg -C Debug --output-on-failure
```

Run the focused tests for the changed module and the repository gates listed in
the pull request template. Tests must use temporary fixtures and must not depend
on user media, settings, devices, installed hosts, or private SDKs.

Before opening a pull request, run the local Inner Source Gates entry:

```powershell
python tools/run_inner_source_gates.py
```

If product code or build inputs change, also run the Debug build and focused
tests above. Do not open a pull request while required gates fail.

## Pull requests

Every pull request requires review before merge. Keep one coherent concern per
pull request, write the title as a user-readable result per the section above,
complete every evidence section in the template, and state any human checks or
remaining risk. Changes affecting two or more domains or a release boundary
require the additional reviewers described by [MAINTAINERS.md](MAINTAINERS.md).

The pull request summary should start with one short sentence in plain Chinese
that says what this change does. Then complete the template sections.

Repository administrators enforce protected-branch and required-check settings.
Contributors must not bypass those controls or merge a failing gate.

## 第三方组件与公开资料

新增或升级第三方组件时，必须同步更新
[`THIRD_PARTY_COMPONENTS.json`](THIRD_PARTY_COMPONENTS.json)，写明来源、版本或提交、
许可证表达式、用途、分发/加载方式、许可证与 NOTICE 文件、风险分类和人工批准引用。
许可证不明、未获评审的 copyleft/商业 SDK，或默认包中出现被排除的 SDK，都会使门禁失败。

只可使用合法取得的公开资料做行为参考。不得抓取或绕过登录、付费墙、技术保护或其他访问控制；
不得把竞品原文、截图、代码或非公开材料提交到公开仓库、PR、Issue、Discussion、文档或源码。文案
是否实质性复制由 reviewer 明确核对，不能用自动文本相似度工具代替人工判断。
资料比对只记录自主概括，并写明来源、访问日期、正常访问方式、观察到的行为、
自己的需求判断、风险判断和记录人。品牌或产品专用标识先交人工核实；必要的兼容性
说明只描述真实关系；行业通用词按上下文判断，不能仅因同名阻断。

修改或发布代码、AI 产物、文案、图片及依赖前，核对创建者、来源、取得方式、
许可证或授权、改动和分发方式。图片等查 ASSET_PROVENANCE.md，
依赖查 THIRD_PARTY_COMPONENTS.json，代码查提交记录。
来源不明时标记“来源待确认”，暂停相关内容的公开提交或发布，交项目负责人及
法务或权利核实人员判断；不得猜测授权或补造记录。

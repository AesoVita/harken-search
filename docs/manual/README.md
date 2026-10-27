# Harken Search 离线图解手册

交付文件：[新手入门与功能手册](HarkenSearch-Manual-zh-CN.pdf)。共 44 页，包括封面、两页可点击目录、软件简介、总界面导览、三页连续上手教程、概念关系页、32 张正式图解、两页常见问题及关键词索引。图片保持原比例与原始像素，不给原图重新画页码。

`learning.json` 保存教程、概念、常见问题与索引；`search-text.json` 保存已确认图片的说明正文与原图坐标，支持 Ctrl+F 和复制文字，且不会重画原图。文字层以图片哈希绑定当前图解，不宣称截图内部每个细小界面字都可检索。操作与文字来源见 [内容核对依据](content-evidence.md)。新增内容仍待用户阅读确认。

唯一图解来源：`documents/harken-search-manual-confirmed/manifest.json` 及其 `settingsSuite`。该目录的网页总览与本 PDF 共用同一套已确认图片。历史编号页、原始截图及工具不进入软件包。

简介与总界面来自用户指定的[《Harken Search 项目简述》](https://xd.feishu.cn/docx/YDDPdaylgoHrsmx3J1fcQnqfneb)。文案与标注位置保存在 `introduction.json`，截图使用 `ui-overview.png`；该两页为新增待审阅内容，不冒充原有 32 张确认图。资料内容及截图哈希参与 PDF 过期检查。

总界面是资料中的真实整屏截图，完整保留界面比例、波形和控件；内网路径、库名、素材名称与第三方封面另存替换为示例，并在导览页标明。原始截图仅本地保留于 `assets/frontend-baseline/harken-search-overview-feishu-20261023.png`，不提交、不打包。可重复的脱敏命令为 `python tools/prepare_manual_overview.py <该原图路径>`；命令校验原图哈希并拒绝覆盖源文件。换图必须先重新核对坐标，再更新来源记录及哈希。

## 生成与检查

在仓库根目录运行：

```powershell
python -m pip install -r tools/requirements-manual.txt
python tools/build_offline_manual.py
python tools/build_offline_manual.py --check
python tools/build_offline_manual.py --stage-dir D:/Aris-QA/manual-package-check
python -m pytest tests/test_offline_manual.py tests/test_manual_learning.py tests/test_pack_engine.py -q --basetemp D:/Aris-QA/manual-tests -p no:cacheprovider
```

只有生成 PDF 与阅读测试需要 PyMuPDF；普通打包校验/复制只使用 Python 标准库。随包路径为 `docs/HarkenSearch-Manual-zh-CN.pdf`，可直接双击离线阅读；使用旧版已有的 `docs` 目录，避免旧更新器因新增父目录返回错误码 30。仓库原稿仍保存在本目录。

自动更新兼容性复验：在 Windows Sandbox 中安装公开原版 1.0.38，保持其原目录结构，不预建 `docs/manual`。从公网发现并下载待验版本后，在真实更新提示中确认重启；核对安装文件与运行中的新版文件 SHA256、版本及签名均与发布包一致，旧进程与更新器退出，事务目录内无文件残留。新更新 ZIP 只应含 `docs/HarkenSearch-Manual-zh-CN.pdf`，不能残留旧随包路径。仅构建成功或下载完成不能代替升级完成。
可重复使用的隔离脚本和输入说明保存在 `tests/manual/public-update/`。

更新原图后先修改正式清单，并核对该图文字层的内容、坐标和图片哈希，再运行生成命令；将变动的正式资料、文案、PDF 和同名 JSON 校验记录一起提交。教程、文字层、图片及清单均参与过期检查，旧 PDF 或缺图会明确失败，不能静默带入。不同系统的 JSON 换行方式不影响校验。

## 原稿恢复记录

2026-10-21 的临时收存已保存未跟踪文件，但随后因 `NUL` 文件清理错误中断，未执行恢复。2026-10-23 从 `197659872e0f6d2e5bd5db8398f43c612f03099c` 的未跟踪资料快照恢复了本手册及其来源；32 张正式图片与原确认清单逐张一致。本次将交付必需的确认资料纳入版本管理，避免仅依赖工作目录和临时收存。

## 发布状态

当前为待用户阅读确认的本地交付；接入打包不代表已经发布新版软件。示例封面与素材是经过脱敏的说明占位图。

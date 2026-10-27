# 公开原版 1.0.38 升级 1.0.39

这两个脚本只在一次性的 Windows Sandbox 内运行，用真实公网更新、真实确认重启和独立文件核对复验错误码 30。它们不修改旧版程序，不预建 `docs/manual`，不使用 QA 专用更新命令。宿主机只提供只读输入和接收输出。

## 准备输入

1. 下载公开原版 1.0.38 更新 ZIP，确认 SHA256 为 `5ad5a5eed0d2f927784dd1d2b7b583251b7d4b3f20016f410891d7bd43899146`，解压为独立 `seed`。核对每个文件与 ZIP 相同，目录中不得有 `docs/manual`。
2. 将本目录两个脚本复制到独立 `input`，并放入 `vc_redist.x64.exe`；脚本核对其 SHA256 为 `cc0ff0eb1dc3f5188ae6300faef32bf5beeba4bdd6e8e445a9184072096b713b`。另备 WebView2 x64 离线安装程序，SHA256 为 `0625fb5c1e75be58a164e14f95aa58a3e5badb859b3d17c952385f3e4a9ff3e8`。
3. 在 `input/expected.json` 写入独立验证过的发布身份，不能从升级后的文件反填预期值：

```json
{
  "oldVersion": "1.0.38",
  "oldExeSha256": "c2faea74911e4c5a391273dad06ffc980dfc3ec3c2832ee453c262dd369a3bb6",
  "newVersion": "1.0.39",
  "newExeSha256": "从已签名正式包独立读取的 EXE SHA256",
  "newPayloadSha256": "正式更新 ZIP SHA256",
  "mainCommit": "本次 main 正式构建提交",
  "manifestUrl": "https://harkensearch.xd.com/aris/releases/latest/download/manifest-win-x64-stable.json",
  "seedSource": "公开原版 1.0.38 更新 ZIP，逐文件核验一致"
}
```

4. 使用 `.wsb` 将 `input`、`seed`、WebView2 安装程序所在目录分别只读映射到 `C:/qa-input`、`C:/qa-seed`、`C:/qa-prerequisite`；仅输出目录以可写方式映射到 `C:/qa-output`。登录命令为 `powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:/qa-input/run-stock.ps1`。必须在全新沙箱运行，不能将脚本直接用于宿主机。

## 执行和判定

脚本在沙箱中部署原版文件、写入隔离安装登记和协议同意夹具，然后启动真实程序，观察其自动发现及下载公网更新。`download-acceptance.json` 通过后，在真实更新提示中点击确认重启。可人工点击，也可由测试操作者先查看 `guest-latest.png`，再向只读输入目录中的 `ui-command.json` 写入以下命令；每条命令使用不同的 `id`，坐标以实际截图为准，禁止盲点固定坐标。

```json
{"id":"view-1","kind":"screenshot"}
```

```json
{"id":"confirm-1","kind":"click","x":100,"y":100}
```

更新事务进行中只观察进程，不反复打开被替换的程序文件。结束后查看 `applied-independent-verification.json`：必须 `passed=true`，新版 EXE 的 SHA256、版本、有效公司签名均匹配，旧进程与更新器退出，新进程运行，事务文件残留为 0，无错误记录。只下载完成、只替换文件或单独出现新版本号都不算通过。

保留 `result.log`、前后截图、下载及最终验收 JSON、正式发布身份和种子核验记录。完成后发送 `close-app` 命令关闭沙箱内本次启动的产品，再关闭并丢弃沙箱；不要关闭宿主机上用户正在使用的软件。

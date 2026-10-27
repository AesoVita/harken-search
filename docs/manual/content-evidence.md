# 新手手册内容核对依据

日期：2026-10-23。操作参考：本卡代码 c34e24f99f2530c84e3ed2a0f3d6c13282993d87，src/Version.h 为 1.0.38。本轮只改文档与 PDF 生成，不修改产品行为；下表是源码与既有资料核对，不冒称新增的真机操作录像。

| 手册内容 | 核对依据 | 边界 |
| --- | --- | --- |
| 浏览模式查看磁盘文件、不要求先入库 | docs/behavior/modules/folder-browser.md；src/main.cpp 的 BeginBrowserPathAudition 与 RenderBrowserSystemFolderContextMenu | 浏览和数据库是两个不同入口 |
| 添加文件夹到音效库 | src/main.cpp:13063 的 Browser_AddFolderToLibrary → QueueBrowserFolderLibraryAdd → PrepareQuickScanDialogForPath | 不假定拖放权限和其他窗口状态，教程选明确菜单入口 |
| 包含子目录、极速与完整扫描、开始扫描 | src/main.cpp:31078 起的扫描窗口；31971 的 Scan_StartScan；locale/harken.ui.strings.json 对应中英文键 | 极速模式的小波形可按需/后台补齐，不以小波形出现与否单独判断入库成功 |
| 顶部搜索按 Enter | src/main.cpp:38135 的 searchFlags/EnterReturnsTrue | 第一次练习在数据库模式搜索确实存在的文件名片段 |
| 播放列表新建及加入 | src/main.cpp:17276 的 PL_NewPlaylist；43591 的 Ctx_AddToPlaylist → database.addManyToPlaylist | 示例为普通具体列表，不把汇总文件夹当实际列表编辑 |
| 从列表移除不删音频 | src/main.cpp:43474；src/Database.cpp:12843 的 removeFromPlaylist 只删除 playlist_items 关联 | 文件系统删除是另一个动作 |
| 从数据库移除与磁盘删除不同 | src/main.cpp:43954 附近 Ctx_RemoveFromDB 和 Ctx_DeleteFromFS，deleteFromFileSystem 分别为 false/true | 不承诺回收站或恢复行为 |
| 在资源管理器显示与复制路径 | src/main.cpp:43442 附近 Edit_RevealInExplorer / Ctx_CopyFilePath；locale/harken.ui.strings.json | 不宣称所有 DAW 已接通；新手用目标软件自己的文件导入方式 |
| 刷新、定位、目录树、波形与其他功能说明 | documents/harken-search-manual-confirmed 的已确认图与配置；数据库文件夹树对应回调 | 仍按已确认功能图说明，不推导未说明的快捷键或性能承诺 |
| 资料编辑不总是只改数据库 | src/main.cpp:11129、11242 调 MetadataWriter::writeAlbumArt；MetadataWriter.cpp | 仅说明已核实的封面写入会影响音频文件，不把全部元数据编辑概括为同一种行为 |
| 无声时检查播放、音量、设备和文件路径 | 已确认波形图与音频播放设置说明 | 为排查顺序，不承诺能解决硬件、驱动和所有格式问题 |

## 搜索文字来源

search-text.json 保存 32 张图对应的 621 段标题和说明正文，以及原图坐标和图片 SHA-256。文字来自已恢复的原始制图配置与设置文案，不以 OCR 猜写。每个图解都绑定正式 manifest 中的图片哈希，正文和坐标变化也会使旧 PDF 过期。

旧清单中的 configSnapshotSha256 与恢复配置的实际字节哈希不符；抽查恢复配置与 Git 临时收存 197659872e0f6d2e5bd5db8398f43c612f03099c^3 中同源文件逐字节一致。不能将旧配置哈希写成已经验证通过。search-text.json 记录本次实际读取的 sourceConfigSha256，正文另与图片检查。32 张原图本身仍与正式图片哈希一致。

PDF 加入不可见文字层，原图像素和可见渲染不改变。全文检索覆盖图解说明正文与设置正文，不宣称截图内部的所有小字都已转录。搜索命中位置使用原图文案坐标；字符宽度按原字体测量。文字层不声称提供完整无障碍标记 PDF。

## 参考与原创范围

REAPER 用户手册只提供信息组织参考：第18页总界面、第59/78页操作与检查、第82–85页概念及任务查询、第14页检索、第459–464页索引。本手册不复制 REAPER 正文、截图或音频。软件简介与总界面继续使用用户指定的飞书资料及已制作的脱敏副本。

## 验收与交付

自动检查：所有 621 段文字提取、搜索命中页和位置、32 张图的嵌入像素与可见渲染、目录/交叉链接/索引、文字或教程变化拒绝旧 PDF、错误文字层或链接不覆盖旧包，以及既有打包路径。

人工阅读：真实生成的新手教程、概念图、问题页、目录和索引预览交用户确认。新增页面与文字层尚未获得用户验收，不能因旧图已确认而直接视为新版整册通过。

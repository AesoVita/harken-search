# Aris 纯手动验收 Checklist

**Status:** Active baseline
**Created:** 2026-06-03
**Last Updated:** 2026-06-03

这份清单给不跑命令、只跑实际软件功能的人用。

原则很简单:

- 只要求你打开 Aris 做真实操作
- 每个模块都写清楚操作步骤
- 每个步骤都写清楚“看到什么算通过”
- 如果某项需要耳朵判断、体感判断、视觉判断，就明确写成手动项，不伪装成自动通过

## 怎么用

### 1. 先确定这次验收属于哪个模块

先看 [module-index.md](module-index.md)，找到本次改动对应模块。

### 2. 打开对应模块契约

每次手动验收前，先看对应 `docs/behavior/modules/*.md`，重点看:

- `## 中文语义`
- `## BDD Scenarios`
- `## Missing Coverage`

这样你知道这次不是只测按钮，而是在测什么用户承诺。

### 3. 按下面的“操作步骤 + 通过信号”走

如果某一步实际现象和通过信号不一致，就记为不通过。

### 4. 结论只写三种

- `PASS`: 手动步骤都符合预期
- `FAIL`: 复现出错误、卡顿、错误状态或语义不一致
- `INCOMPLETE_VERIFICATION`: 这次改动命中了该模块，但你还没走到对应手动步骤

## 全局手动验收

适用场景:

- 你只想开软件看真实效果
- 你不跑命令行
- 你要做“产品经理签字式”验收

建议顺序:

1. 启动 Aris
2. 打开测试库或日常使用库
3. 按本次涉及模块逐项验收
4. 把结果按文末模板记录

## 模块手动验收表

### `search-query`

看什么:

- 搜索框输入后的语义是否稳定
- chips 是否和输入含义一致
- 删除一个 chip 时是否只删除对应语义

操作步骤:

1. 在搜索框输入混合查询，例如关键词 + 引号词 + 字段过滤
2. 观察生成的 chips
3. 删除其中一个 chip
4. 再看剩余查询结果是否仍保留其余语义

通过信号:

- chips 文义和输入一致
- 删除一个 chip 后，不会把无关条件一起删掉
- 不会出现“看起来一样、结果却乱跳”的感觉

### `search-execution`

看什么:

- 搜索结果是否和筛选条件一致
- 多条件组合后结果是否明显跑偏

操作步骤:

1. 输入一个你知道应有结果的搜索词
2. 再加一个筛选条件
3. 再移除该筛选条件
4. 对比前后结果是否符合直觉

通过信号:

- 加条件后结果收窄
- 去掉条件后结果恢复
- 不会出现明显错误的结果集或空结果

### `scan-indexing`

看什么:

- 导入或重连库后，文件是否真的进入可搜索状态
- 长时间扫描时 UI 是否还可理解
- “入库完成”是不是只在真正可立即工作时才出现

操作步骤:

1. 添加一个新库或重连一个已存在库
2. 触发扫描
3. 观察扫描过程中的进度、状态文案、列表变化
4. 扫描后立刻搜索新导入文件名

通过信号:

- 扫描过程不是“卡死不动”
- 扫描完成后新文件能被搜到
- 不会出现“扫描看似完成但结果没有进库”的现象

本轮新增关注:

- 等界面明确显示“入库完成”后，立刻滚动当前结果列表 1 到 2 屏
- 立刻打开封面墙并观察首屏
- 随机点几个刚进入可见区的文件，确认主波形进入可检查状态
- 如果后台还在继续做全库预热，状态语义应是“后台优化中”之类，而不是提前宣告入库完成

新增通过信号:

- 只有在当前结果已经可直接工作时才显示“入库完成”
- 入库完成后首轮滚动当前结果列表时，不会因为补 mini 波形出现明显停顿
- 入库完成后首屏封面墙已经是去重后的稳定状态，不会一边看一边大面积重排

本轮 `极速模式` 长库最终验收:

1. 选择真实用户库，不用 900 文件 fixture，优先用这次实际卡过的 SoundMorph / BoomLibrary 之类大库。
2. 在扫描弹窗里明确选择 `极速模式`，开始一次真实扫描或重连。
3. 等界面第一次明确出现“本次入库完成”或可点击 `Done`。
4. 不要等待后台全部安静，立刻连续滚动当前结果列表 2 到 3 屏。
5. 立刻打开封面墙，看首屏是否已经稳定。
6. 立刻连续点击 5 个之前没看过的文件，观察当前文件主波形是否都切到正确内容。
7. 如果后台仍显示优化中，再等待 20 到 30 秒，重复 1 屏滚动和 2 次切行。

本轮 `极速模式` 通过信号:

- “本次入库完成”出现前后，不会出现 1 秒以上的整窗假死
- 首轮滚动不会反复被 mini 波形补齐打成一顿一顿
- 封面墙首屏不会出现大面积重复、空白抖动、或明显重排后才稳定
- 连续点文件时，主波形会切到当前文件，不会长期停在旧文件或空白状态
- 如果 mini 波形仍在后台补齐，它表现为渐进补全，而不是整批刷新把滚动手感拖死

本轮 `极速模式` 不通过信号:

- 刚显示完成就立刻卡住，必须等很久才能滚动或点文件
- 首轮滚动出现连续明显卡顿，像被后台 mini 批量补齐抢住
- 封面墙首屏在可见区域内持续大面积重排、去重不稳、或长时间空白
- 当前文件已经切换，但主波形明显跟不上、串旧文件、或经常卡很久不出来
- 以上任一项说不清、没验到、或结果不稳定，都保持 `INCOMPLETE_VERIFICATION`

### `waveform`

看什么:

- 可见行的 mini waveform 是否很快出现
- 当前选中文件的主 waveform 是否最终收敛到正确内容
- 切文件时是否串旧波形
- 入库完成后首轮滚动时，mini waveform 是否还会拖慢浏览

操作步骤:

1. 搜索出一批结果并快速滚动列表
2. 观察可见行的 mini waveform
3. 连续点选多个文件
4. 停在目标文件上，看主 waveform 是否最终正确显示

通过信号:

- 可见行不会长期空白
- 主 waveform 不会一直停在旧文件内容
- 快速切文件时不会明显串屏或错位

本轮新增关注:

- 先完成一次真实导入或重连，并等到界面显示“入库完成”
- 入库完成后立即滚动一批当前结果
- 连续点选多个文件，包含刚进入可见区、之前还没看过的文件

新增通过信号:

- 入库完成后，可见行 mini waveform 不会在首轮滚动时造成明显卡顿
- 如果某些 mini waveform 仍在补齐，它们必须表现为平滑的渐进补全，而不是阻塞列表滚动或触发整批刷新

取消回滚检查:

1. 开始一次真实扫描，等到列表里已经能看到本次新导入的一部分文件
2. 在扫描还没结束前点击 `Cancel`
3. 等扫描状态变成已取消，再关闭扫描弹窗并回到原库视图

取消回滚通过信号:

- 本次扫描新出现的文件不会残留在库里
- 已有文件不会因为这次取消扫描留下新的 mini waveform / cover backfill 污染
- 不会自动切到刚才扫描的路径，也不会保留“本次已入库”的假完成状态

### `cover`

看什么:

- 封面墙、筛选封面、当前预览是否是同一个封面语义
- 滚动或点击时是否出现明显错误封面

操作步骤:

1. 打开封面墙
2. 点击一个封面或封面相关筛选
3. 看筛选结果和当前预览
4. 快速滚动几屏再回看

通过信号:

- 点中的封面和筛选语义一致
- 当前预览不会跳成无关图片
- 不会出现大量封面空白、乱串、错位

### `cover-replacement-browse-surface`

What to look for:

- One replacement browse surface owns folder tree, keyword, path, and cover filtering without scope drift.
- The selected tile, current preview, and info panel stay on the same cover identity.
- Tile presets or slider changes relayout the wall without resetting the active scope.

Manual steps:

1. Open a fixture or real library that contains several cover-bearing folders.
2. Enter the artwork / replacement browse surface.
3. Type a keyword such as `WHOOSH` or another known local hit and confirm the wall narrows immediately.
4. Select a folder tree node and confirm the wall, counts, and bottom path info still match the active keyword scope.
5. Click one cover tile to pin a same-cover filter and confirm the visible wall, current preview, and top/bottom info still point to the same identity family.
6. Switch `Compact`, `Browse`, and `Inspect`, or drag the size slider, and confirm the wall keeps the same scope instead of resetting.
7. Clear keyword, tree/path, and cover filters one lane at a time and confirm the remaining active lanes stay in effect until they are explicitly cleared.

Pass signals:

- No contradictory wall reset when keyword/tree/path/cover lanes are combined.
- Current preview does not jump to an unrelated file or legacy identity after refresh.
- Top/bottom info remains readable and consistent with the visible wall after tile-size changes.
- The wall feels like one Aris-style surface instead of multiple independent filter lanes fighting each other.

### `playback-library-lifecycle`

看什么:

- 播放时切文件是否稳定
- 切库后是否残留旧音频、旧波形、旧封面、旧结果

操作步骤:

1. 播放一个文件
2. 切到另一个文件再播放
3. 切换数据库或库
4. 观察当前播放、当前信息区、当前波形、当前列表是否都切到新库

通过信号:

- 不会听到旧文件还在继续
- 不会看到旧库内容假装还是当前内容
- 切库后界面状态是一致的

### `metadata-tags`

看什么:

- 元数据编辑后显示是否正确
- tag 增删改后是否立即反映在相关区域

操作步骤:

1. 打开一个有明确元数据的文件
2. 修改一个字段或 tag
3. 保存或应用
4. 切到别的条目再切回来

通过信号:

- 修改结果能稳定显示
- 不会出现刚改完正确、切一下又回旧值
- tag 不会莫名丢失或串到别的记录

### `playlist-organization`

看什么:

- 播放列表层级、移动、拖放是否符合直觉
- 父子关系是否被错误改坏

操作步骤:

1. 新建或选中一个播放列表
2. 尝试移动到另一个父级下
3. 展开/收起父级
4. 切换别的列表再回来

通过信号:

- 结构不会乱
- 父子层级和你刚才操作一致
- 不会出现“界面看着成功，其实位置没变”的情况

### `delivery-export`

看什么:

- 导出、交付、发送动作是否真的完成
- 失败时是否给出可理解反馈

操作步骤:

1. 选中文件或片段
2. 执行导出或交付动作
3. 看导出结果文件或目标位置
4. 再试一个故意容易失败的场景，例如无效路径或非法条件

通过信号:

- 成功时能在目标位置看到正确结果
- 失败时不会静默没反应
- 不会出现“UI 显示成功但实际上没导出”的情况

### `settings-startup`

看什么:

- 启动是否清晰、稳定
- 设置是否真的保存并在下次启动生效

操作步骤:

1. 修改一个明显可见的设置
2. 关闭 Aris
3. 重新打开
4. 检查设置是否保留，以及启动过程是否有异常状态

通过信号:

- 设置能保留
- 启动后状态可理解，不会莫名空白、丢库、错状态
- 不会有“刚启动就做了奇怪的大动作”的感受

## 手动验收记录模板

```md
## 手动验收结论

- 模块: `cover`
- 验收方式: 纯手动软件操作
- 操作步骤:
  - 打开封面墙
  - 点击封面筛选
  - 快速滚动后回看当前预览
- 通过信号:
  - 封面筛选语义正确
  - 当前预览不串图
  - 快速滚动无明显错封面
- 结果:
  - `PASS` / `FAIL` / `INCOMPLETE_VERIFICATION`
- 备注:
  - 若失败，写出你看到的实际现象
```

## 什么时候优先用这份清单

- 你不会跑命令
- 你要验真实产品行为
- 你要做视觉、交互、听感、体感层面的判断

这份是“人验产品”的入口；命令行那份 [acceptance-checklist.md](acceptance-checklist.md) 仍保留给工程侧使用。

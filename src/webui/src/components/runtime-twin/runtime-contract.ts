export type RuntimeRect = Readonly<{ x: number; y: number; width: number; height: number }>;

export type RuntimeRegion = Readonly<{
  rect: RuntimeRect;
  source: string;
  runtimeRole: string;
}>;

// Explicit user-approved deviations from the native renderer. Source fidelity
// is the default, but these accepted web corrections must survive every later
// calibration pass unless the user explicitly changes them.
export const ARIS_RUNTIME_APPROVED_WEB_OVERRIDES = {
  removeDuplicateBordersAndGaps: true,
  removeToolbarNoResults: true,
  fillFileTableAndWaveform: true,
  workspaceUFrameBelowTab: true,
  removePresetActiveUnderline: true,
  preserveAcceptedLayoutAndOptionSpacing: true,
} as const;

export const ARIS_RUNTIME_REFERENCE = {
  capturedAt: '2026-07-21',
  executable: 'build/Release/Aris.exe',
  windowTitle: 'Aris - JUSTMIXRFull (0 文件)',
  width: 2576,
  height: 1408,
  sourceRoot: 'src/main.cpp',
  regions: {
    chrome: { rect: { x: 0, y: 0, width: 2576, height: 96 }, source: 'src/main.cpp RenderUI: menu bar and Toolbar child', runtimeRole: 'title, menu, navigation, search and product mode' },
    sidebar: { rect: { x: 8, y: 96, width: 516, height: 1042 }, source: 'src/main.cpp RenderUI: Filter and folder browser panels', runtimeRole: 'related paths and system folder tree' },
    workspace: { rect: { x: 536, y: 96, width: 1490, height: 1042 }, source: 'src/main.cpp:30291,43740 left/right 12px splitters; RenderUI center results', runtimeRole: 'file browser and searchable result list' },
    info: { rect: { x: 2038, y: 96, width: 530, height: 1042 }, source: 'src/main.cpp RenderFileInfoPanelContent', runtimeRole: 'metadata, artwork and transfer/play history' },
    transport: { rect: { x: 0, y: 1140, width: 2576, height: 260 }, source: 'approved web simplification: full-width transport; native anchor is src/main.cpp RenderUI', runtimeRole: 'audition controls, waveform, meter and REAPER handoff' },
  } satisfies Record<string, RuntimeRegion>,
} as const;

// FileTableV6 is intentionally modeled from the native implementation rather
// than from screenshot measurements. Column indexes and saved widths come from
// src/main.cpp:46211-46245 and the active imgui.ini table 0x1A9D093B. The
// horizontal cell padding comes from src/Theme.cpp:249.
export const ARIS_FILE_TABLE_RUNTIME = {
  source: {
    columns: 'src/main.cpp:46211-46245',
    savedLayout: 'imgui.ini:[Table][0x1A9D093B,23]',
    cellPadding: 'src/Theme.cpp:249',
  },
  cellPaddingX: 8,
  columns: [
    { id: 'cover', label: 'c...', sourceIndex: 17, order: 0, savedWidth: 30 },
    { id: 'filename', label: 'Filename', sourceIndex: 0, order: 1, savedWidth: 394 },
    { id: 'waveform', label: 'Waveform', sourceIndex: 1, order: 2, savedWidth: 306 },
    { id: 'duration', label: 'Duration', sourceIndex: 2, order: 3, savedWidth: 88 },
    { id: 'format', label: 'Format', sourceIndex: 3, order: 4, savedWidth: 50 },
    { id: 'sampleRate', label: 'Sample Rate', sourceIndex: 5, order: 6, savedWidth: 77 },
    { id: 'channels', label: 'Channels', sourceIndex: 8, order: 9, savedWidth: 61 },
    { id: 'category', label: 'Category', sourceIndex: 9, order: 10, savedWidth: 60 },
    { id: 'folderPath', label: 'Folder Path', sourceIndex: 16, order: 17, savedWidth: 1178 },
  ],
} as const;

export const ARIS_FILE_TABLE_GRID = ARIS_FILE_TABLE_RUNTIME.columns
  .map(column => `${column.savedWidth + ARIS_FILE_TABLE_RUNTIME.cellPaddingX * 2}px`)
  .join(' ');

export const RUNTIME_RELATED_PATHS = ['CD2', 'CD3', 'CD4', 'RO2_UI_录屏', 'test', '天使波利', '摇滚鲨虫需求包', '海神', '源'];

export const RUNTIME_FOLDER_TREE = [
  ['E:\\', 0], ['H:\\', 0], ['$RECYCLE.BIN', 1], ['Mis', 1], ['Originals', 1],
  ['PhasePlant Prest', 1], ['Python脚本', 1], ['QYWX', 1], ['SERUM PRESET', 1],
  ['Sound Design', 1], ['Sound effect', 1], ['System Volume Information', 1], ['WeGameApps', 1],
  ['XD_Workspace', 1], ['XD_私人定制', 1], ['新建文件夹', 1], ['游戏工程', 1], ['迅雷下载', 1],
    ['音效制作文档和录屏', 1], ['CD2', 2], ['CD3', 2], ['CD4', 2], ['RO2_UI_录屏', 2], ['test', 2],
    ['天使波利', 2], ['游戏内', 3], ['游戏外', 3],
    ['摇滚键虫需求包', 2], ['20004', 3], ['20004游戏内', 3], ['优化', 3], ['参考音乐', 3],
] as const;

export const RUNTIME_FILE_ROWS = Array.from({ length: 32 }, (_, index) => ({
  name: index === 16
    ? 'pakchunk0-WindowsClient_005177_00000A537835'
    : `pakchunk0-WindowsClient_${String(5161 + index).padStart(6, '0')}_00000A4A${(0x1835 + 0x1800 * index).toString(16).toUpperCase().padStart(4, '0')}`,
  format: '.wav',
  folder: 'D:\\Valorant-WAV\\pakchunk0-WindowsClient',
  selected: index === 16,
}));

export const RUNTIME_ARTWORK = Array.from({ length: 58 }, (_, index) => ({
  id: index,
  hue: (index * 37) % 360,
  title: ['LIGHT', 'FUTURE', 'MECHANISM', 'APPIRE', 'RESONANCE'][index % 5],
}));

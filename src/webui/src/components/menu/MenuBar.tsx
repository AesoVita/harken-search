import React, { useMemo, useState, useCallback, useEffect, useRef } from 'react';
import DropdownMenu, { DropdownMenuItem } from './DropdownMenu';
import type { AppSettingsDto } from '../../types';

export interface RecentDbInfo {
  current: string;
  items: string[];
}

export interface PlaylistInfo {
  id: number;
  name: string;
}

export interface MenuBarProps {
  onAction: (method: string, params?: Record<string, unknown>) => void;
  recentDbs: RecentDbInfo;
  playlists: PlaylistInfo[];
  settings: AppSettingsDto;
  selectedId: number | null;
  fileCount: number;
}

type MenuKey = 'file' | 'edit' | 'view' | 'database' | 'playlist' | 'tools' | 'help' | null;

const menuLabels: { key: Exclude<MenuKey, null>; label: string }[] = [
  { key: 'file', label: '文件' },
  { key: 'edit', label: '编辑' },
  { key: 'view', label: '视图' },
  { key: 'database', label: '数据库' },
  { key: 'playlist', label: '播放列表' },
  { key: 'tools', label: '工具' },
  { key: 'help', label: '帮助' },
];

function shortDbName(path: string): string {
  const parts = path.replace(/\\/g, '/').split('/');
  return parts[parts.length - 1] || path;
}

export default function MenuBar({
  onAction,
  recentDbs,
  playlists,
  settings,
  selectedId,
  fileCount,
}: MenuBarProps) {
  const [openMenu, setOpenMenu] = useState<MenuKey>(null);
  const barRef = useRef<HTMLDivElement>(null);
  const triggerRefs = useRef<Record<string, HTMLButtonElement | null>>({});
  const closeTimerRef = useRef<number>(0);

  const cancelClose = useCallback(() => {
    clearTimeout(closeTimerRef.current);
  }, []);

  const scheduleClose = useCallback(() => {
    clearTimeout(closeTimerRef.current);
    closeTimerRef.current = window.setTimeout(() => setOpenMenu(null), 220);
  }, []);

  // Close menu when clicking outside
  useEffect(() => {
    if (!openMenu) return;
    const onDown = (e: PointerEvent) => {
      if (barRef.current && !barRef.current.contains(e.target as Node)) {
        setOpenMenu(null);
      }
    };
    window.addEventListener('pointerdown', onDown, true);
    return () => window.removeEventListener('pointerdown', onDown, true);
  }, [openMenu]);

  const act = useCallback(
    (method: string, params?: Record<string, unknown>) => {
      setOpenMenu(null);
      onAction(method, params);
    },
    [onAction],
  );

  // ── 文件 ──
  const fileItems = useMemo<DropdownMenuItem[]>(() => {
    const items: DropdownMenuItem[] = [
      { id: 'open-folder', label: '打开文件夹...', shortcut: 'Ctrl+O', onClick: () => act('menu.openFolder') },
      { id: 'scan-folder', label: '扫描到数据库...', shortcut: 'Ctrl+Shift+S', onClick: () => act('menu.scanFolder') },
      { id: 'sep2', label: '', separator: true },
      { id: 'exit', label: '退出', shortcut: 'Alt+F4', onClick: () => act('menu.exit') },
    ];
    return items;
  }, [act]);

  // ── 编辑 ──
  const editItems = useMemo<DropdownMenuItem[]>(() => {
    const hasSelection = selectedId !== null && selectedId > 0;
    return [
      {
        id: 'edit-filename',
        label: '编辑文件名',
        shortcut: 'E',
        disabled: !hasSelection,
        onClick: () => act('menu.editFilename', { id: selectedId }),
      },
      { id: 'sep1', label: '', separator: true },
      { id: 'reset-pitch', label: '重置音高', shortcut: '=', onClick: () => act('transport.setPitch', { pitch: 1.0 }) },
      { id: 'sep2', label: '', separator: true },
      { id: 'random-sort', label: '随机排列', onClick: () => act('menu.randomSort') },
      { id: 'random-next', label: '随机下一个', shortcut: 'Shift+→', onClick: () => act('transport.playRandom') },
      { id: 'sep3', label: '', separator: true },
      {
        id: 'reveal-explorer',
        label: '在资源管理器显示',
        shortcut: 'Ctrl+R',
        disabled: !hasSelection,
        onClick: () => act('fileList.contextAction', { id: selectedId, action: 'revealInExplorer' }),
      },
      { id: 'sep4', label: '', separator: true },
      {
        id: 'autoplay',
        label: '自动播放',
        checked: !!settings.autoplayOnSelect,
        onClick: () => act('menu.toggleSetting', { key: 'autoplayOnSelect' }),
      },
      {
        id: 'continuous-play',
        label: '连续播放',
        checked: !!settings.continuousPlayback,
        onClick: () => act('menu.toggleSetting', { key: 'continuousPlayback' }),
      },
      {
        id: 'shuffle-play',
        label: '随机播放',
        checked: !!settings.shufflePlayback,
        disabled: !settings.continuousPlayback,
        onClick: () => act('menu.toggleSetting', { key: 'shufflePlayback' }),
      },
      {
        id: 'player-link',
        label: '播放器联动',
        checked: !!settings.playerSelectionLink,
        onClick: () => act('menu.toggleSetting', { key: 'playerSelectionLink' }),
      },
      { id: 'sep5', label: '', separator: true },
      { id: 'refresh', label: '刷新', shortcut: 'F5', onClick: () => act('menu.refresh') },
    ];
  }, [act, selectedId, settings]);

  // ── 视图 ──（尽量扁平化：避免二级子菜单造成信息不清楚）
  const viewItems = useMemo<DropdownMenuItem[]>(
    () => [
      { id: 'sec-wf', label: '', sectionLabel: '波形显示' },
      { id: 'wf-classic', label: '经典绿色', checked: true, disabled: true, onClick: () => act('menu.saveSettings', {}) },
      { id: 'wf-freq', label: '频率着色', disabled: true, onClick: () => act('menu.saveSettings', {}) },
      { id: 'sep-view0', label: '', separator: true },
      { id: 'spectrogram', label: '频谱图', checked: !!settings.showSpectrogram, onClick: () => act('menu.toggleSetting', { key: 'showSpectrogram' }) },
      {
        id: 'use-webui-main',
        label: '使用 WebUI 主界面（重启生效）',
        checked: !!settings.useWebUIMain,
        onClick: () => act('menu.toggleSetting', { key: 'useWebUIMain' }),
      },
      { id: 'sep-view1', label: '', separator: true },
      { id: 'sec-panels', label: '', sectionLabel: '面板' },
      { id: 'panel-sidebar', label: '筛选/文件夹', disabled: true },
      { id: 'panel-info', label: '文件信息', disabled: true },
      { id: 'panel-wave', label: '波形面板', disabled: true },
      { id: 'sep-panel', label: '', separator: true },
      { id: 'reset-layout', label: '重置布局', onClick: () => act('menu.resetLayout') },
      { id: 'sep-view2', label: '', separator: true },
      { id: 'play-history', label: '播放历史', disabled: true },
      { id: 'clear-history', label: '清除历史', onClick: () => act('menu.clearPlayHistory') },
    ],
    [act, settings.showSpectrogram, settings.useWebUIMain],
  );

  // ── 数据库 ──
  const databaseItems = useMemo<DropdownMenuItem[]>(() => {
    const items: DropdownMenuItem[] = [];

    if (recentDbs.current) {
      items.push({
        id: 'db-current',
        label: `当前数据库：${shortDbName(recentDbs.current)}`,
        disabled: true,
      });
      items.push({ id: 'sep-db-cur', label: '', separator: true });
    }

    items.push(
      { id: 'db-open', label: '打开数据库...', shortcut: 'Ctrl+O', onClick: () => act('menu.openDb') },
      { id: 'db-new', label: '新建数据库...', shortcut: 'Ctrl+N', onClick: () => act('menu.newDb') },
    );

    if (recentDbs.items.length) {
      items.push({ id: 'sep-db-recent', label: '', separator: true });
      items.push({ id: 'db-recent-label', label: '', sectionLabel: '最近使用' });
      items.push(
        ...recentDbs.items.map((p, i) => ({
          id: `db-recent-${i}`,
          label: shortDbName(p),
          checked: p === recentDbs.current,
          disabled: p === recentDbs.current,
          onClick: () => act('menu.switchDb', { path: p }),
        })),
      );
      items.push(
        { id: 'sep-db-recent2', label: '', separator: true },
        { id: 'db-clear-recent', label: '清除最近列表', onClick: () => act('menu.clearRecentDbs') },
      );
    }

    items.push(
      { id: 'sep-db1', label: '', separator: true },
      { id: 'db-scan', label: '扫描文件夹...', onClick: () => act('menu.scanFolder') },
      { id: 'sep-db2', label: '', separator: true },
      { id: 'db-count', label: `共 ${fileCount} 个文件`, disabled: true },
    );

    return items;
  }, [act, recentDbs, fileCount]);

  // ── 播放列表 ──
  const playlistItems = useMemo<DropdownMenuItem[]>(() => {
    const items: DropdownMenuItem[] = [
      { id: 'pl-new', label: '新建播放列表...', onClick: () => act('sidebar.createPlaylist') },
    ];
    if (playlists.length) {
      items.push({ id: 'sep-pl', label: '', separator: true });
      playlists.forEach((pl) => {
        items.push({
          id: `pl-${pl.id}`,
          label: pl.name,
          onClick: () => act('menu.selectPlaylist', { id: pl.id }),
        });
      });
    }
    return items;
  }, [act, playlists]);

  // ── 工具 ──
  const toolsItems = useMemo<DropdownMenuItem[]>(() => [
    { id: 'tools-sec1', label: '', sectionLabel: '── 分类与标签 ──' },
    { id: 'tools-ucs', label: 'UCS 分类浏览器...', disabled: true },
    { id: 'tools-tags', label: '标签系统...', disabled: true },
    { id: 'tools-fields', label: '字段管理...', disabled: true },
    { id: 'sep-tools1', label: '', separator: true },
    { id: 'tools-sec2', label: '', sectionLabel: '── 元数据处理 ──' },
    { id: 'tools-embed', label: '元数据嵌入...', disabled: true },
    { id: 'tools-naming', label: '命名脚本...', disabled: true },
    { id: 'tools-workflow', label: '工作流引擎...', disabled: true },
    { id: 'sep-tools2', label: '', separator: true },
    { id: 'tools-sec3', label: '', sectionLabel: '── 文件操作 ──' },
    { id: 'tools-transfer', label: '文件传输/转换...', disabled: true },
    { id: 'tools-autorename', label: '文件夹自动重命名...', disabled: true },
    { id: 'sep-tools3', label: '', separator: true },
    { id: 'tools-sec4', label: '', sectionLabel: '── 外观 ──' },
    { id: 'tools-theme', label: '高级主题编辑器...', disabled: true },
    { id: 'sep-tools4', label: '', separator: true },
    { id: 'tools-ai', label: 'AI 面板', disabled: true },
    { id: 'sep-tools5', label: '', separator: true },
    { id: 'tools-settings', label: '应用设置...', disabled: true },
    { id: 'tools-shortcuts', label: '快捷键设置...', disabled: true },
  ], []);

  // ── 帮助 ──
  const helpItems = useMemo<DropdownMenuItem[]>(() => [
    { id: 'about', label: '关于 Harken Search', onClick: () => act('menu.about') },
  ], [act]);

  const allMenus: Record<Exclude<MenuKey, null>, DropdownMenuItem[]> = useMemo(
    () => ({
      file: fileItems,
      edit: editItems,
      view: viewItems,
      database: databaseItems,
      playlist: playlistItems,
      tools: toolsItems,
      help: helpItems,
    }),
    [fileItems, editItems, viewItems, databaseItems, playlistItems, toolsItems, helpItems],
  );

  return (
    <div className="f-menu-row" ref={barRef}>
      {menuLabels.map(({ key, label }) => (
        <div className="menu-trigger-wrap" key={key}>
          <button
            className={`f-menu-btn${openMenu === key ? ' f-menu-btn-open' : ''}`}
            ref={(el) => {
              triggerRefs.current[key] = el;
            }}
            onClick={() => setOpenMenu(openMenu === key ? null : key)}
            onPointerEnter={() => {
              cancelClose();
              if (openMenu !== key) setOpenMenu(key);
            }}
            onPointerLeave={() => {
              scheduleClose();
            }}
          >
            {label}
          </button>
          <DropdownMenu
            open={openMenu === key}
            items={allMenus[key]}
            anchorEl={triggerRefs.current[key] ?? null}
            onHoverChange={(hovered) => {
              if (hovered) cancelClose();
              else scheduleClose();
            }}
            onClose={() => setOpenMenu(null)}
          />
        </div>
      ))}
    </div>
  );
}

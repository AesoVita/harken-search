import React, { useEffect, useRef, useState } from 'react';
import { ARIS_FILE_TABLE_GRID, ARIS_FILE_TABLE_RUNTIME, RUNTIME_FILE_ROWS } from './runtime-contract';

type Props = { style: React.CSSProperties; globalQuery: string; committedBrowserPath: string; onNavigateBrowserPath: (path: string) => void };
type WorkspaceTab = { id: number; name: string; count: number; locked: boolean; color: number };
const tableStyle = { '--rt-file-table-columns': ARIS_FILE_TABLE_GRID } as React.CSSProperties;
const workspaceTabPalette = ['#ff5050', '#ffa028', '#ffdc28', '#50c850', '#3ca0ff', '#a050ff', '#ff78c8', '#8c8c8c'];
const tableContentWidth = ARIS_FILE_TABLE_RUNTIME.columns.reduce(
  (width, column) => width + column.savedWidth + ARIS_FILE_TABLE_RUNTIME.cellPaddingX * 2,
  0,
);

function MiniWave({ index, strong = false }: { index: number; strong?: boolean }) {
  return <span className="mini-wave-fixture" data-row-index={index} data-source="src/main.cpp: RenderMiniWaveform"><svg viewBox="0 0 250 28" preserveAspectRatio="none" aria-hidden="true"><path d={strong ? 'M0 14 L8 14 10 4 12 24 15 8 18 20 21 2 24 25 28 7 33 20 39 5 45 22 52 9 60 17 70 11 82 16 96 12 112 15 130 13 155 14 188 14 250 14' : 'M0 14 L35 14 45 12 52 16 60 10 66 17 72 12 80 16 87 13 95 15 110 14 250 14'} /></svg></span>;
}

export default function RuntimeWorkspace({ style, globalQuery, committedBrowserPath, onNavigateBrowserPath }: Props) {
  const [selectedIndex, setSelectedIndex] = useState(16);
  const [workspaceTabs, setWorkspaceTabs] = useState<WorkspaceTab[]>([{ id: 1, name: 'All Files', count: 0, locked: false, color: -1 }]);
  const [activeWorkspaceTabId, setActiveWorkspaceTabId] = useState(1);
  const [tabPresetMenuOpen, setTabPresetMenuOpen] = useState(false);
  const [tabContextId, setTabContextId] = useState<number | null>(null);
  const [renamingTabId, setRenamingTabId] = useState<number | null>(null);
  const [draggedTabId, setDraggedTabId] = useState<number | null>(null);
  const [renameBuffer, setRenameBuffer] = useState('');
  const nextWorkspaceTabId = useRef(2);
  const [includeSubdirectories, setIncludeSubdirectories] = useState(false);
  const [audioOnly, setAudioOnly] = useState(true);
  const [browserPath, setBrowserPath] = useState(committedBrowserPath);
  const [query, setQuery] = useState('');
  const [tableViewportWidth, setTableViewportWidth] = useState(0);
  const [tableViewportHeight, setTableViewportHeight] = useState(0);
  const [tableScrollPx, setTableScrollPx] = useState(0);
  const [tableScrollY, setTableScrollY] = useState(0);
  const fileTableRef = useRef<HTMLDivElement>(null);
  const maxTableScroll = Math.max(0, tableContentWidth - tableViewportWidth);
  const scrollTrackWidth = Math.max(0, tableViewportWidth - 4);
  const scrollThumbWidth = maxTableScroll > 0
    ? Math.max(24, scrollTrackWidth * Math.min(1, tableViewportWidth / tableContentWidth))
    : scrollTrackWidth;
  const scrollThumbTravel = Math.max(0, scrollTrackWidth - scrollThumbWidth);
  const scrollThumbLeft = maxTableScroll > 0 ? (tableScrollPx / maxTableScroll) * scrollThumbTravel : 0;
  const queryTokens = `${globalQuery} ${query}`.trim().toLowerCase().split(/\s+/).filter(Boolean);
  const visibleRows = RUNTIME_FILE_ROWS.filter(row => queryTokens.every(token => row.name.toLowerCase().includes(token)));
  useEffect(() => setBrowserPath(committedBrowserPath), [committedBrowserPath]);
  useEffect(() => {
    const table = fileTableRef.current;
    if (!table) return;
    const updateViewportSize = () => {
      setTableViewportWidth(table.clientWidth);
      setTableViewportHeight(table.clientHeight);
    };
    updateViewportSize();
    const observer = new ResizeObserver(updateViewportSize);
    observer.observe(table);
    return () => observer.disconnect();
  }, []);
  useEffect(() => {
    setTableScrollPx(current => Math.min(current, maxTableScroll));
  }, [maxTableScroll]);
  const tableBodyViewportHeight = Math.max(0, tableViewportHeight - 44);
  const tableBodyContentHeight = visibleRows.length * 32;
  const maxTableScrollY = Math.max(0, tableBodyContentHeight - tableBodyViewportHeight);
  const verticalTrackHeight = Math.max(0, tableBodyViewportHeight - 4);
  const verticalThumbHeight = maxTableScrollY > 0
    ? Math.max(24, verticalTrackHeight * Math.min(1, tableBodyViewportHeight / tableBodyContentHeight))
    : verticalTrackHeight;
  const verticalThumbTravel = Math.max(0, verticalTrackHeight - verticalThumbHeight);
  const verticalThumbTop = maxTableScrollY > 0 ? (tableScrollY / maxTableScrollY) * verticalThumbTravel : 0;
  useEffect(() => {
    setTableScrollY(current => Math.min(current, maxTableScrollY));
  }, [maxTableScrollY]);
  const navigateToParentDirectory = () => {
    const normalized = browserPath.replace(/[\\/]+$/, '');
    const separator = Math.max(normalized.lastIndexOf('\\'), normalized.lastIndexOf('/'));
    if (separator < 0) return;
    const parent = separator === 2 && normalized[1] === ':' ? normalized.slice(0, 3) : normalized.slice(0, separator);
    if (parent) {
      setBrowserPath(parent);
      onNavigateBrowserPath(parent);
    }
  };
  const commitBrowserPath = () => onNavigateBrowserPath(browserPath);
  const handleBrowserPathKeyDown = (event: React.KeyboardEvent<HTMLInputElement>) => {
    if (event.key === 'Enter') {
      commitBrowserPath();
      event.currentTarget.blur();
    }
  };
  const handleBrowserFilterKeyDown = (event: React.KeyboardEvent<HTMLInputElement>) => {
    if (event.key === 'Escape') {
      setQuery('');
      event.currentTarget.blur();
    }
  };
  const handleTableScrollPointerDown = (event: React.PointerEvent<HTMLDivElement>) => {
    if (maxTableScroll <= 0 || scrollThumbTravel <= 0) return;
    const track = event.currentTarget;
    const startX = event.clientX;
    const startScroll = tableScrollPx;
    track.setPointerCapture(event.pointerId);
    const handlePointerMove = (moveEvent: PointerEvent) => {
      const nextScroll = startScroll + ((moveEvent.clientX - startX) / scrollThumbTravel) * maxTableScroll;
      setTableScrollPx(Math.max(0, Math.min(maxTableScroll, nextScroll)));
    };
    const stopDragging = () => {
      track.removeEventListener('pointermove', handlePointerMove);
      track.removeEventListener('pointerup', stopDragging);
      track.removeEventListener('pointercancel', stopDragging);
    };
    track.addEventListener('pointermove', handlePointerMove);
    track.addEventListener('pointerup', stopDragging);
    track.addEventListener('pointercancel', stopDragging);
  };
  const handleTableWheel = (event: React.WheelEvent<HTMLDivElement>) => {
    if (event.ctrlKey || maxTableScrollY <= 0) return;
    event.preventDefault();
    setTableScrollY(current => Math.max(0, Math.min(maxTableScrollY, current + event.deltaY)));
  };
  const handleTableVerticalScrollPointerDown = (event: React.PointerEvent<HTMLDivElement>) => {
    if (maxTableScrollY <= 0 || verticalThumbTravel <= 0) return;
    const track = event.currentTarget;
    const startY = event.clientY;
    const startScroll = tableScrollY;
    track.setPointerCapture(event.pointerId);
    const handlePointerMove = (moveEvent: PointerEvent) => {
      const nextScroll = startScroll + ((moveEvent.clientY - startY) / verticalThumbTravel) * maxTableScrollY;
      setTableScrollY(Math.max(0, Math.min(maxTableScrollY, nextScroll)));
    };
    const stopDragging = () => {
      track.removeEventListener('pointermove', handlePointerMove);
      track.removeEventListener('pointerup', stopDragging);
      track.removeEventListener('pointercancel', stopDragging);
    };
    track.addEventListener('pointermove', handlePointerMove);
    track.addEventListener('pointerup', stopDragging);
    track.addEventListener('pointercancel', stopDragging);
  };
  const createWorkspaceTab = () => {
    const id = nextWorkspaceTabId.current++;
    setWorkspaceTabs(current => [...current, { id, name: 'All Files', count: 0, locked: false, color: -1 }]);
    setActiveWorkspaceTabId(id);
    setQuery('');
  };
  const duplicateWorkspaceTab = (tabId: number) => {
    const source = workspaceTabs.find(tab => tab.id === tabId);
    if (!source) return;
    const id = nextWorkspaceTabId.current++;
    setWorkspaceTabs(current => [...current, { ...source, id, name: `${source.name} copy`, locked: false }]);
    setActiveWorkspaceTabId(id);
    setTabContextId(null);
  };
  const closeWorkspaceTab = (tabId: number) => {
    if (workspaceTabs.length <= 1) return;
    const index = workspaceTabs.findIndex(tab => tab.id === tabId);
    const remaining = workspaceTabs.filter(tab => tab.id !== tabId);
    setWorkspaceTabs(remaining);
    if (activeWorkspaceTabId === tabId) setActiveWorkspaceTabId(remaining[Math.min(index, remaining.length - 1)].id);
    setTabContextId(null);
  };
  const handleWorkspaceTabAuxClick = (event: React.MouseEvent, tabId: number) => {
    if (event.button === 1) closeWorkspaceTab(tabId);
  };
  const beginWorkspaceTabRename = (tab: WorkspaceTab) => {
    setRenameBuffer(tab.name);
    setRenamingTabId(tab.id);
    setTabContextId(null);
  };
  const commitWorkspaceTabRename = () => {
    const nextName = renameBuffer.trim();
    if (renamingTabId !== null && nextName) setWorkspaceTabs(current => current.map(tab => tab.id === renamingTabId ? { ...tab, name: nextName } : tab));
    setRenamingTabId(null);
  };
  const handleTabDragStart = (event: React.DragEvent<HTMLButtonElement>, tabId: number) => {
    setDraggedTabId(tabId);
    event.dataTransfer.effectAllowed = 'move';
  };
  const handleTabDrop = (event: React.DragEvent<HTMLButtonElement>, targetTabId: number) => {
    event.preventDefault();
    if (draggedTabId === null || draggedTabId === targetTabId) return;
    setWorkspaceTabs(current => {
      const sourceIndex = current.findIndex(tab => tab.id === draggedTabId);
      const targetIndex = current.findIndex(tab => tab.id === targetTabId);
      if (sourceIndex < 0 || targetIndex < 0) return current;
      const reordered = [...current];
      const [moved] = reordered.splice(sourceIndex, 1);
      reordered.splice(targetIndex, 0, moved);
      return reordered;
    });
    setDraggedTabId(null);
  };
  const contextTab = workspaceTabs.find(tab => tab.id === tabContextId);
  return <section className="runtime-region runtime-workspace" style={style} data-source="src/main.cpp: browser tabs, controls and FileTableV6">
    <div className="workspace-tabbar" data-source="src/main.cpp:44275-44760" data-reorder-source="src/main.cpp:44275 ImGuiTabBarFlags_Reorderable">
      {workspaceTabs.map(tab => renamingTabId === tab.id
        ? <input className="workspace-tab workspace-tab-rename" value={renameBuffer} onChange={event => setRenameBuffer(event.target.value)} onBlur={commitWorkspaceTabRename} onKeyDown={event => { if (event.key === 'Enter') commitWorkspaceTabRename(); if (event.key === 'Escape') setRenamingTabId(null); }} autoFocus key={tab.id} />
        : <button type="button" className={`workspace-tab ${activeWorkspaceTabId === tab.id ? 'active' : ''} ${tab.locked ? 'locked' : ''} ${draggedTabId === tab.id ? 'dragging' : ''}`} style={tab.color >= 0 ? { '--rt-tab-color': workspaceTabPalette[tab.color] } as React.CSSProperties : undefined} onClick={() => setActiveWorkspaceTabId(tab.id)} onAuxClick={event => handleWorkspaceTabAuxClick(event, tab.id)} onContextMenu={event => { event.preventDefault(); setTabContextId(tab.id); setTabPresetMenuOpen(false); }} draggable onDragStart={event => handleTabDragStart(event, tab.id)} onDragOver={event => event.preventDefault()} onDrop={event => handleTabDrop(event, tab.id)} onDragEnd={() => setDraggedTabId(null)} key={tab.id}>[目录] {tab.name} ({tab.count})</button>)}
      <button type="button" className="workspace-tab-add" onClick={createWorkspaceTab} aria-label="New tab">+</button>
    </div>
    <button type="button" className="tab-preset-button" onClick={() => setTabPresetMenuOpen(open => !open)} aria-expanded={tabPresetMenuOpen} aria-label="Tab presets">☆</button>
    {tabPresetMenuOpen && <div className="tab-preset-menu"><button type="button" onClick={() => setTabPresetMenuOpen(false)}>Save current layout</button><button type="button" onClick={() => setTabPresetMenuOpen(false)}>Default</button></div>}
    {contextTab && <div className="workspace-tab-menu" data-source="src/main.cpp:44372-44427">
      <button type="button" onClick={() => beginWorkspaceTabRename(contextTab)}>Rename</button>
      <button type="button" onClick={() => duplicateWorkspaceTab(contextTab.id)}>Duplicate workspace</button>
      <div className="workspace-tab-colors">{workspaceTabPalette.map((color, index) => <button type="button" style={{ background: color }} onClick={() => { setWorkspaceTabs(current => current.map(tab => tab.id === contextTab.id ? { ...tab, color: index } : tab)); setTabContextId(null); }} aria-label={`Tab color ${index + 1}`} key={color} />)}</div>
      <button type="button" onClick={() => { setWorkspaceTabs(current => current.map(tab => tab.id === contextTab.id ? { ...tab, locked: !tab.locked } : tab)); setTabContextId(null); }}>{contextTab.locked ? 'Unlock page' : 'Lock page'}</button>
      <button type="button" disabled={workspaceTabs.length <= 1} onClick={() => closeWorkspaceTab(contextTab.id)}>Close tab</button>
      <button type="button" disabled={workspaceTabs.length <= 1} onClick={() => { setWorkspaceTabs([contextTab]); setActiveWorkspaceTabId(contextTab.id); setTabContextId(null); }}>Close others</button>
    </div>}
    <div className="path-row" data-source="src/main.cpp:45127-45187"><b>Path:</b><button type="button" onClick={navigateToParentDirectory}>Parent Directory</button><input className="browser-path-input" value={browserPath} onChange={event => setBrowserPath(event.target.value)} onKeyDown={handleBrowserPathKeyDown} onBlur={() => setBrowserPath(committedBrowserPath)} aria-label="Browser path" /><button type="button" onClick={commitBrowserPath}>Open</button></div>
    <div className="browser-options"><button className="browser-toggle" onClick={() => setIncludeSubdirectories(false)}><span className={`radio ${!includeSubdirectories ? 'on' : ''}`} />当前目录</button><button className="browser-toggle" onClick={() => setIncludeSubdirectories(true)}><span className={`radio ${includeSubdirectories ? 'on' : ''}`} />含子目录音频</button><button className="browser-toggle" onClick={() => setAudioOnly(value => !value)}><span className={`check ${audioOnly ? 'on' : ''}`}>{audioOnly ? '✓' : ''}</span>Audio Only</button><small>(11781)</small><input value={query} onChange={event => setQuery(event.target.value)} onKeyDown={handleBrowserFilterKeyDown} placeholder="搜索文件名（多词用空格分隔）..." aria-label="Search file names" /></div>
    <div ref={fileTableRef} className="file-table" style={tableStyle} onWheel={handleTableWheel} data-layout-source={ARIS_FILE_TABLE_RUNTIME.source.savedLayout} data-source="src/main.cpp:46155 FileTableV6">
      <div className="file-scroll-plane" style={{ width: tableContentWidth, transform: `translateX(${-tableScrollPx}px)` }}>
        <div className="file-head">{ARIS_FILE_TABLE_RUNTIME.columns.map(column => <span key={column.id}>{column.label}</span>)}</div>
        <div className="file-body"><div className="file-body-plane" style={{ transform: `translateY(${-tableScrollY}px)` }}>{visibleRows.map((row, index) => <div className={`file-row ${selectedIndex === RUNTIME_FILE_ROWS.indexOf(row) ? 'selected' : ''}`} onClick={() => setSelectedIndex(RUNTIME_FILE_ROWS.indexOf(row))} data-row-index={RUNTIME_FILE_ROWS.indexOf(row)} key={row.name}>
          <span /><span className={index === 1 ? 'amber' : ''}>{row.name}</span><span className="mini-wave"><MiniWave index={RUNTIME_FILE_ROWS.indexOf(row)} strong={row.selected} /></span><span>-</span><span>{row.format}</span><span>-</span><span>-</span><span>-</span><span className="file-path">{row.folder}</span>
        </div>)}</div></div>
      </div>
      <div className="file-v-scroll" onPointerDown={handleTableVerticalScrollPointerDown} data-source="src/main.cpp:46155-46166 FileTableV6 ScrollY"><span style={{ height: verticalThumbHeight, transform: `translateY(${verticalThumbTop}px)` }} /></div>
      <div className="h-scroll" onPointerDown={handleTableScrollPointerDown}><span style={{ width: scrollThumbWidth, transform: `translateX(${scrollThumbLeft}px)` }} /></div>
    </div>
  </section>;
}

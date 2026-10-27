import React, { useMemo, useRef, useState } from 'react';
import type { AudioRecord, SearchFilters } from '../../types';

type SortColumn = 'fileName' | 'format' | 'duration' | 'sampleRate';

interface FileListPanelProps {
  rows: AudioRecord[];
  rowHeight: number;
  sortColumn: SortColumn;
  sortAsc: boolean;
  onSort: (column: SortColumn) => void;
  selectedIds: Set<number>;
  onSelectRow: (row: AudioRecord, index: number, e: React.MouseEvent<HTMLButtonElement>) => void;
  onPlayRow: (row: AudioRecord) => void;
  onContextAction: (row: AudioRecord, action: string) => void;
  onLoadMore: () => void;
  hasMore: boolean;
  filters: SearchFilters;
}

interface TabItem {
  id: number;
  name: string;
}

export default function FileListPanel(props: FileListPanelProps) {
  const {
    rows,
    rowHeight,
    sortColumn,
    sortAsc,
    onSort,
    selectedIds,
    onSelectRow,
    onPlayRow,
    onContextAction,
    onLoadMore,
    hasMore,
    filters,
  } = props;

  const [tabs, setTabs] = useState<TabItem[]>([{ id: 1, name: '全部文件' }]);
  const [activeTab, setActiveTab] = useState(1);
  const [renamingTab, setRenamingTab] = useState<number | null>(null);
  const [context, setContext] = useState<{ x: number; y: number; row: AudioRecord } | null>(null);
  const [scrollTop, setScrollTop] = useState(0);
  const listRef = useRef<HTMLDivElement | null>(null);

  const viewportHeight = 420;
  const overscan = 8;
  const totalHeight = rows.length * rowHeight;
  const startIdx = Math.max(0, Math.floor(scrollTop / rowHeight) - overscan);
  const endIdx = Math.min(rows.length, Math.ceil((scrollTop + viewportHeight) / rowHeight) + overscan);

  const visibleRows = useMemo(() => rows.slice(startIdx, endIdx), [rows, startIdx, endIdx]);

  const addTab = () => {
    const id = Math.max(...tabs.map((t) => t.id)) + 1;
    const next = [...tabs, { id, name: `标签 ${id}` }];
    setTabs(next);
    setActiveTab(id);
  };

  const closeTab = (id: number) => {
    if (tabs.length <= 1) return;
    const next = tabs.filter((t) => t.id !== id);
    setTabs(next);
    if (activeTab === id) setActiveTab(next[0].id);
  };

  return (
    <div className="file-list-panel">
      <div className="tabs-bar">
        {tabs.map((tab) => (
          <div key={tab.id} className={`tab-chip ${activeTab === tab.id ? 'tab-chip-active' : ''}`}>
            {renamingTab === tab.id ? (
              <input
                className="tab-rename-input"
                defaultValue={tab.name}
                autoFocus
                onBlur={(e) => {
                  const name = e.target.value.trim() || tab.name;
                  setTabs((old) => old.map((it) => (it.id === tab.id ? { ...it, name } : it)));
                  setRenamingTab(null);
                }}
              />
            ) : (
              <button className="tab-chip-btn" onClick={() => setActiveTab(tab.id)} onDoubleClick={() => setRenamingTab(tab.id)}>
                {tab.name}
              </button>
            )}
            <button className="tab-close-btn" onClick={() => closeTab(tab.id)}>×</button>
          </div>
        ))}
        <button className="tab-add-btn" onClick={addTab}>+</button>
      </div>

      <div className="records-header-row records-header-row-tight">
        <button className="header-btn" onClick={() => onSort('fileName')}>文件名 {sortColumn === 'fileName' ? (sortAsc ? '▲' : '▼') : ''}</button>
        <button className="header-btn" onClick={() => onSort('format')}>格式 {sortColumn === 'format' ? (sortAsc ? '▲' : '▼') : ''}</button>
        <button className="header-btn" onClick={() => onSort('duration')}>时长 {sortColumn === 'duration' ? (sortAsc ? '▲' : '▼') : ''}</button>
        <button className="header-btn" onClick={() => onSort('sampleRate')}>采样率 {sortColumn === 'sampleRate' ? (sortAsc ? '▲' : '▼') : ''}</button>
      </div>

      <div
        className="records-list records-list-virtual"
        ref={listRef}
        style={{ height: `${viewportHeight}px` }}
        onScroll={(e) => setScrollTop(e.currentTarget.scrollTop)}
        onContextMenu={(e) => e.preventDefault()}
      >
        <div style={{ position: 'relative', height: `${totalHeight}px` }}>
          {visibleRows.map((r, i) => {
            const index = startIdx + i;
            const top = index * rowHeight;
            const selected = selectedIds.has(r.id);
            return (
              <button
                key={r.id}
                className={`record-row record-row-virtual ${selected ? 'record-row-selected' : ''}`}
                style={{ position: 'absolute', top: `${top}px`, height: `${rowHeight}px` }}
                onClick={(e) => onSelectRow(r, index, e)}
                onDoubleClick={() => onPlayRow(r)}
                onContextMenu={(e) => {
                  e.preventDefault();
                  setContext({ x: e.clientX, y: e.clientY, row: r });
                }}
              >
                <span className="record-col-name">
                  <span className={`played-dot ${index % 7 === 0 ? 'played-dot-on' : ''}`} />
                  {r.fileName}
                </span>
                <span>{r.format || '-'}</span>
                <span>{Math.round(r.duration)}s</span>
                <span>{r.sampleRate}Hz</span>
              </button>
            );
          })}
        </div>
      </div>

      <div className="file-list-footer">
        <span>过滤: {Object.keys(filters).filter((k) => (filters as any)[k]).length} 项</span>
        {hasMore && (
          <button className="btn-secondary btn-compact" onClick={onLoadMore}>
            加载更多
          </button>
        )}
      </div>

      {context && (
        <div className="context-menu" style={{ left: `${context.x}px`, top: `${context.y}px` }} onMouseLeave={() => setContext(null)}>
          <button onClick={() => { onContextAction(context.row, 'play'); setContext(null); }}>播放</button>
          <button onClick={() => { onContextAction(context.row, 'stop'); setContext(null); }}>停止</button>
          <button onClick={() => { onContextAction(context.row, 'revealInExplorer'); setContext(null); }}>在资源管理器显示</button>
          <button onClick={() => { onContextAction(context.row, 'removeFromDb'); setContext(null); }}>从数据库移除</button>
        </div>
      )}
    </div>
  );
}

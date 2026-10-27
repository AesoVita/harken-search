import React, { useMemo, useState } from 'react';
import type { FolderNode, PlaylistItem } from '../../types';

type ViewMode = 'database' | 'browser' | 'playlist';

interface SidebarPanelProps {
  folderTree: FolderNode | null;
  playlists: PlaylistItem[];
  onFilterFolder: (path: string) => void;
  onApplyFilters: (payload: Record<string, unknown>) => void;
  onCreatePlaylist: (name: string) => void;
}

function FolderTreeNode({ node, depth, onClick }: { node: FolderNode; depth: number; onClick: (path: string) => void }) {
  const [open, setOpen] = useState(depth < 1);
  const hasChildren = node.subfolders?.length > 0;
  return (
    <div>
      <button className="folder-node-btn" style={{ paddingLeft: `${8 + depth * 14}px` }} onClick={() => onClick(node.path)}>
        <span className="folder-node-arrow" onClick={(e) => { e.stopPropagation(); setOpen((v) => !v); }}>
          {hasChildren ? (open ? '▾' : '▸') : '·'}
        </span>
        <span className="folder-node-name">{node.name || '(root)'}</span>
        <span className="folder-node-count">{node.fileCount}</span>
      </button>
      {open && hasChildren && node.subfolders.map((c) => (
        <FolderTreeNode key={c.path} node={c} depth={depth + 1} onClick={onClick} />
      ))}
    </div>
  );
}

export default function SidebarPanel(props: SidebarPanelProps) {
  const { folderTree, playlists, onFilterFolder, onApplyFilters, onCreatePlaylist } = props;
  const [viewMode, setViewMode] = useState<ViewMode>('database');
  const [artist, setArtist] = useState('');
  const [album, setAlbum] = useState('');
  const [category, setCategory] = useState('');
  const [newList, setNewList] = useState('');

  const modeLabel = useMemo(
    () => ({
      database: '数据库',
      browser: '浏览器',
      playlist: '播放列表',
    }[viewMode]),
    [viewMode],
  );

  return (
    <div className="sidebar-panel-root">
      <div className="view-mode-row">
        {(['database', 'browser', 'playlist'] as ViewMode[]).map((m) => (
          <button key={m} className={`view-mode-btn ${viewMode === m ? 'view-mode-btn-active' : ''}`} onClick={() => setViewMode(m)}>
            {m === 'database' ? '🗄' : m === 'browser' ? '📁' : '≡'}
          </button>
        ))}
        <span className="view-mode-label">{modeLabel}</span>
      </div>

      <div className="sidebar-section">
        <div className="sidebar-section-title">文件夹树</div>
        <div className="folder-tree-scroll">
          {folderTree ? <FolderTreeNode node={folderTree} depth={0} onClick={onFilterFolder} /> : <div className="panel-placeholder">加载中...</div>}
        </div>
      </div>

      <div className="sidebar-section">
        <div className="sidebar-section-title">元数据过滤</div>
        <div className="sidebar-filter-grid">
          <input className="glass-input" placeholder="艺术家" value={artist} onChange={(e) => setArtist(e.target.value)} />
          <input className="glass-input" placeholder="专辑" value={album} onChange={(e) => setAlbum(e.target.value)} />
          <input className="glass-input" placeholder="分类" value={category} onChange={(e) => setCategory(e.target.value)} />
          <button className="btn-secondary btn-compact" onClick={() => onApplyFilters({ artist, album, category })}>应用过滤</button>
        </div>
      </div>

      <div className="sidebar-section">
        <div className="sidebar-section-title">播放列表</div>
        <div className="playlist-list">
          {playlists.map((p) => (
            <div className="playlist-item" key={p.id}>
              <span>{p.name}</span>
            </div>
          ))}
        </div>
        <div className="playlist-create-row">
          <input className="glass-input" placeholder="新建播放列表" value={newList} onChange={(e) => setNewList(e.target.value)} />
          <button
            className="btn-secondary btn-compact"
            onClick={() => {
              if (!newList.trim()) return;
              onCreatePlaylist(newList.trim());
              setNewList('');
            }}
          >
            新建
          </button>
        </div>
      </div>
    </div>
  );
}

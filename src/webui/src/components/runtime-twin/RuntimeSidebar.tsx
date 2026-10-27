import React, { useState } from 'react';
import { RUNTIME_FOLDER_TREE, RUNTIME_RELATED_PATHS } from './runtime-contract';

type Props = { style: React.CSSProperties };
// settings.ini: order=0,5,1,4,2,3 with ViewMode hidden => 5,1,4 | 2,3.
const railIcons = ['spark', 'branch', 'tag', 'funnel', 'list'] as const;
const railSectionIds = [5, 1, 4, 2, 3] as const;
const railSectionLabels: Record<(typeof railSectionIds)[number], string> = {
  5: 'Quick Paths',
  1: 'Folder Tree',
  4: 'Tag Cloud',
  2: 'Metadata',
  3: 'Playlists',
};

function RailGlyph({ name }: { name: typeof railIcons[number] }) {
  const paths: Record<string, React.ReactNode> = {
    branch: <><path d="M4 2v12M4 7h7M4 13h7" /><circle cx="4" cy="2" r="1" fill="currentColor" /><circle cx="11" cy="7" r="1" fill="currentColor" /><circle cx="11" cy="13" r="1" fill="currentColor" /></>,
    funnel: <path d="M2 2h12L9 7v6l-2 1V7z" />,
    list: <path d="M2 4h2M7 4h7M2 8h2M7 8h7M2 12h2M7 12h7" />,
    tag: <><path d="M2 2h6l6 6-6 6-6-6z" /><circle cx="5" cy="5" r="1" fill="currentColor" /></>,
    spark: <path d="M9 1 3 8h5l-1 7 6-8H8z" />,
  };
  return <svg className="rail-glyph" viewBox="0 0 16 16" aria-hidden="true" fill="none" stroke="currentColor" strokeWidth="1.2" strokeLinecap="round" strokeLinejoin="round">{paths[name]}</svg>;
}

function SidebarToolGlyph({ locate = false }: { locate?: boolean }) {
  return <svg className="sidebar-tool-glyph" viewBox="0 0 16 16" aria-hidden="true" fill="none" stroke="currentColor" strokeWidth="1.2" strokeLinecap="round" strokeLinejoin="round">{locate ? <><circle cx="8" cy="8" r="5.5" /><circle cx="8" cy="8" r="1.5" fill="currentColor" stroke="none" /></> : <><path d="M13 6V2.5L10.5 5" /><path d="M13 5a5 5 0 1 0 1 5" /></>}</svg>;
}

function SidebarPanelMenuGlyph() {
  return <svg className="rail-glyph" viewBox="0 0 20 20" aria-hidden="true" fill="none" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round">
    <path d="M3 5h14M3 10h14M3 15h14" />
  </svg>;
}

function SidebarQuickPaths() {
  const [selected, setSelected] = useState(0);
  const paths = [
    'HUD高科技感UI界面交互点击选择滑动无损音效',
    '[虚幻引擎终极音效包] Unreal Engine - Ultimate SFX Bundle',
  ];
  return <div className="sidebar-section-body quick-paths" data-source="src/main.cpp:19912-19958">{paths.map((path, index) => <button type="button" className={selected === index ? 'selected' : ''} onClick={() => setSelected(index)} key={path}><span>⌁</span>{path}</button>)}</div>;
}

function SidebarFolderTree() {
  const [selectedFolder, setSelectedFolder] = useState('');
  const [folderQuery, setFolderQuery] = useState('');
  const visibleFolders = RUNTIME_FOLDER_TREE.filter(([name]) => name.toLowerCase().includes(folderQuery.trim().toLowerCase()));
  const treeMarker = (name: string, depth: number, index: number) => {
    if (index === 0) return '▸';
    if (index === 1 || name === '音效制作文档和录屏' || name === '天使波利' || name === '摇滚键虫需求包') return '▾';
    if (depth === 1 || name === 'RO2_UI_录屏') return '▸';
    return '';
  };
  return <div className="sidebar-section-body folder-section" data-source="src/main.cpp:19960-20361">
    <div className="related-title">Related Paths · H:\音效制作文档和录屏</div>
    <div className="related-list">{RUNTIME_RELATED_PATHS.map(path => <div className={selectedFolder === path ? 'selected' : ''} onClick={() => setSelectedFolder(path)} key={path}>{path}</div>)}</div>
    <div className="folder-search"><input value={folderQuery} onChange={event => setFolderQuery(event.target.value)} placeholder="Search folders..." aria-label="Search folders" /><button aria-label="Refresh folder tree"><SidebarToolGlyph /></button><button aria-label="Locate current folder"><SidebarToolGlyph locate /></button></div>
    <div className="tree-title">System Folder Tree · H:\音效制作文档和录屏</div>
    <div className="folder-tree">{visibleFolders.map(([name, depth], i) => <div className={`tree-row ${selectedFolder === name ? 'selected' : ''}`} onClick={() => setSelectedFolder(name)} style={{ paddingLeft: 13 + depth * 20 }} key={`${name}-${i}`}><span>{treeMarker(name, depth, i)}</span>{name}</div>)}</div>
  </div>;
}

function SidebarTagCloud() {
  const [selectedTags, setSelectedTags] = useState<Set<string>>(() => new Set());
  const tags = ['UI', 'Impact', 'Whoosh', 'Metal', 'Mechanical', 'Weapon', 'Ambience', 'Footstep', 'Magic', 'Creature', 'Vehicle', 'Alarm', 'Click', 'Transition'];
  const toggle = (tag: string) => setSelectedTags(current => {
    const next = new Set(current);
    if (next.has(tag)) next.delete(tag); else next.add(tag);
    return next;
  });
  return <div className="sidebar-section-body tag-cloud" data-source="src/main.cpp:20363-20590">{tags.map((tag, index) => <button type="button" className={selectedTags.has(tag) ? 'selected' : ''} style={{ fontSize: `${12 + index % 3}px` }} onClick={() => toggle(tag)} key={tag}>{tag}</button>)}</div>;
}

function SidebarMetadataFilters() {
  const fields = ['Audio Type', 'Sample Rate', 'Channels', 'Duration', 'Category', 'Library', 'BPM', 'Musical Key'];
  const [enabled, setEnabled] = useState<Set<string>>(() => new Set(['Audio Type', 'Category']));
  return <div className="sidebar-section-body metadata-filters" data-source="src/main.cpp:20592-21238">{fields.map(field => <label key={field}><input type="checkbox" checked={enabled.has(field)} onChange={() => setEnabled(current => { const next = new Set(current); if (next.has(field)) next.delete(field); else next.add(field); return next; })} /><span>{field}</span><em>Any</em></label>)}</div>;
}

function SidebarPlaylists() {
  const [selected, setSelected] = useState('Recently Played');
  const playlists = ['Recently Played', 'Sound Design', 'Game UI', 'To Review'];
  return <div className="sidebar-section-body playlists-section" data-source="src/main.cpp:21240">{playlists.map(name => <button type="button" className={selected === name ? 'selected' : ''} onClick={() => setSelected(name)} key={name}><span>☷</span>{name}</button>)}</div>;
}

export default function RuntimeSidebar({ style }: Props) {
  const [activeSectionId, setActiveSectionId] = useState<number>(1);
  const [panelMenuOpen, setPanelMenuOpen] = useState(false);
  const [visibleSections, setVisibleSections] = useState<Set<number>>(() => new Set(railSectionIds));
  const toggleSectionVisibility = (sectionId: number) => {
    setVisibleSections(current => {
      const next = new Set(current);
      if (next.has(sectionId)) {
        next.delete(sectionId);
        if (activeSectionId === sectionId) setActiveSectionId(-1);
      } else {
        next.add(sectionId);
      }
      return next;
    });
  };
  const renderActiveSection = () => {
    switch (activeSectionId) {
      case 5: return <SidebarQuickPaths />;
      case 1: return <SidebarFolderTree />;
      case 4: return <SidebarTagCloud />;
      case 2: return <SidebarMetadataFilters />;
      case 3: return <SidebarPlaylists />;
      default: return null;
    }
  };
  return <aside className="runtime-region runtime-sidebar" style={style} data-source="src/main.cpp: DrawSidebarActivityBar and folder views">
    <div className="panel-caption">Filter</div>
    <div className="sidebar-main">
      <div className="sidebar-rail">{railIcons.map((x, i) => {
        const sectionId = railSectionIds[i];
        if (!visibleSections.has(sectionId)) return null;
        return <span className={`${sectionId === activeSectionId ? 'active' : ''} ${i === 3 ? 'rail-group-break' : ''}`} onClick={() => setActiveSectionId(current => current === sectionId ? -1 : sectionId)} role="button" tabIndex={0} aria-label={railSectionLabels[sectionId]} key={x}><RailGlyph name={x} /></span>;
      })}<button type="button" className="sidebar-panel-menu-button" aria-label="Manage modules" aria-expanded={panelMenuOpen} onClick={() => setPanelMenuOpen(open => !open)} data-source="src/main.cpp:19438-19501"><SidebarPanelMenuGlyph /></button>
        {panelMenuOpen && <div className="sidebar-panel-menu" role="menu">
          <button type="button" role="menuitem">Float</button>
          <button type="button" role="menuitem">Hide</button>
          <i />
          <small>Show / hide modules</small>
          {railSectionIds.map(sectionId => <label key={sectionId}><input type="checkbox" checked={visibleSections.has(sectionId)} onChange={() => toggleSectionVisibility(sectionId)} />{railSectionLabels[sectionId]}</label>)}
        </div>}
      </div>
      <div className={`sidebar-content ${activeSectionId < 0 ? 'collapsed' : ''}`}>
        {renderActiveSection()}
      </div>
    </div>
  </aside>;
}

import React, { useState } from 'react';
import { RUNTIME_ARTWORK } from './runtime-contract';

type Props = { style: React.CSSProperties };
const tabs = ['Metadata Summary', 'Artwork', 'Transfer History', 'Play History'] as const;
const metadata = [
  ['Audio Type', 'WAV'], ['Bit Depth', '16 bit'], ['Channels', 'Stereo'], ['Sample Rate', '48000 Hz'],
  ['Duration', '00:00:05.20'], ['Filename', 'pakchunk0-WindowsClient_005177_00000A537835.wav'],
  ['Path', 'D:\\Valorant-WAV\\pakchunk0-WindowsClient'],
] as const;

export default function RuntimeInfoPanel({ style }: Props) {
  const [activeTab, setActiveTab] = useState<(typeof tabs)[number]>('Artwork');
  const [thumbSize, setThumbSize] = useState(60);
  const [metadataEditMode, setMetadataEditMode] = useState(false);
  const [infoPanelMenuOpen, setInfoPanelMenuOpen] = useState(false);
  const [infoPanelVisible, setInfoPanelVisible] = useState(true);
  const [infoPanelFloating, setInfoPanelFloating] = useState(false);
  const [editableMetadata, setEditableMetadata] = useState({ description: '', keywords: '', category: '' });
  const artworkGridStyle = { '--rt-art-thumb': `${thumbSize}px` } as React.CSSProperties;
  const handleInfoTitleContextMenu = (event: React.MouseEvent<HTMLDivElement>) => {
    event.preventDefault();
    setInfoPanelMenuOpen(true);
  };
  const toggleMetadataEditMode = () => {
    setMetadataEditMode(current => !current);
    setActiveTab('Metadata Summary');
    setInfoPanelMenuOpen(false);
  };
  if (!infoPanelVisible) return null;
  return <aside className={`runtime-region runtime-info ${infoPanelFloating ? 'floating' : ''}`} style={style} data-source="src/main.cpp: RenderFileInfoPanelContent">
    <div className="info-caption" onContextMenu={handleInfoTitleContextMenu} data-source="src/main.cpp:49544-49698">
      <span className="info-caption-label">Info</span>
      <button type="button" className="info-edit-button" onClick={toggleMetadataEditMode} aria-pressed={metadataEditMode} aria-label={metadataEditMode ? 'Exit metadata edit mode' : 'Edit metadata'}>
        <svg viewBox="0 0 18 18" aria-hidden="true"><path d="M12.7 3.4l1.9 1.9M4 14l2.5-.6 7.3-7.3-1.9-1.9-7.3 7.3L4 14z" /></svg>
      </button>
      {infoPanelMenuOpen && <div className="info-title-menu" role="menu">
        <button type="button" role="menuitem" onClick={() => { setInfoPanelFloating(true); setInfoPanelMenuOpen(false); }}>Float</button>
        <button type="button" role="menuitem" onClick={() => setInfoPanelVisible(false)}>Hide</button>
      </div>}
    </div>
    <div className="info-tabs" role="tablist">{tabs.map(tab => <button className={activeTab === tab ? 'active' : ''} onClick={() => setActiveTab(tab)} role="tab" aria-selected={activeTab === tab} key={tab}>{tab}</button>)}</div>
    {activeTab === 'Artwork'
      ? <div className="art-grid" style={artworkGridStyle} data-source="src/main.cpp:22804-24539 artwork wall">{RUNTIME_ARTWORK.map((art, index) => <div className="cover" style={{ '--h': art.hue } as React.CSSProperties} key={art.id}><i>{index % 7 === 0 ? 'SOUND' : index % 5 === 0 ? 'FUTURE' : ''}</i><b>{art.title}</b><small>{index % 4 === 0 ? 'COLLECTION' : ''}</small></div>)}</div>
      : activeTab === 'Metadata Summary'
        ? <div className="metadata-summary">{metadata.map(([label, value]) => <div className="metadata-row" key={label}><b>{label}</b><span>{value}</span></div>)}
          {metadataEditMode && <div className="metadata-editor" data-source="src/main.cpp:49612-49668 metadataEditMode">
            <label>Description<input value={editableMetadata.description} onChange={event => setEditableMetadata(current => ({ ...current, description: event.target.value }))} /></label>
            <label>Keywords<input value={editableMetadata.keywords} onChange={event => setEditableMetadata(current => ({ ...current, keywords: event.target.value }))} /></label>
            <label>Category<input value={editableMetadata.category} onChange={event => setEditableMetadata(current => ({ ...current, category: event.target.value }))} /></label>
          </div>}
        </div>
        : <div className="info-empty-state">{activeTab === 'Transfer History' ? 'Transfer History is unavailable in this build.' : 'No play history.'}</div>}
    {activeTab === 'Artwork' && <div className="artwork-footer" data-source="src/main.cpp:24497-24531"><input type="range" min={56} max={160} step={1} value={thumbSize} onChange={event => setThumbSize(Number(event.target.value))} aria-label="Artwork thumbnail size" /></div>}
  </aside>;
}

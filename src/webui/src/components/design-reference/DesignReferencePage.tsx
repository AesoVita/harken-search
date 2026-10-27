import React, { useRef, useState } from 'react';
import './design-reference.css';

type PanelKey = 'sidebar' | 'info' | 'waveform';

const folders = ['Sound effect', 'Foley', 'UI', 'Ambience', 'Voice', 'Music', 'Weapons', 'Footsteps', 'Impacts'];
const rows = [
  ['Hit_Punch_01-03', '00:00:14.22', 'WAV', '48 kHz', 'SFX'],
  ['Hit_Punch_01-04', '00:00:17.45', 'WAV', '48 kHz', 'SFX'],
  ['Hit_Punch_01-05', '00:00:19.20', 'WAV', '48 kHz', 'SFX'],
  ['Hit_Punch_01-06', '00:00:14.22', 'WAV', '48 kHz', 'SFX'],
  ['Hit_Punch_01-07', '00:00:21.33', 'WAV', '48 kHz', 'SFX'],
  ['Hit_Punch_01-08', '00:00:20.11', 'WAV', '48 kHz', 'SFX'],
  ['Hit_Punch_01-09', '00:00:13.71', 'WAV', '48 kHz', 'SFX'],
  ['Hit_Punch_01-10', '00:00:18.03', 'WAV', '48 kHz', 'SFX'],
  ['Hit_Punch_01-11', '00:00:15.08', 'WAV', '48 kHz', 'SFX'],
  ['Hit_Punch_01-12', '00:00:16.82', 'WAV', '48 kHz', 'SFX'],
];

function Grip({ direction, onPointerDown }: { direction: 'vertical' | 'horizontal'; onPointerDown: (e: React.PointerEvent) => void }) {
  return <button className={`reference-grip grip-${direction}`} aria-label={`Resize ${direction} panel`} onPointerDown={onPointerDown}><span /><span /><span /></button>;
}

function Waveform() {
  return <div className="reference-waveform" aria-label="Waveform preview">{Array.from({ length: 88 }, (_, i) => <i key={i} style={{ height: `${12 + ((i * 37) % 54)}%` }} />)}</div>;
}

export default function DesignReferencePage() {
  const [mode, setMode] = useState<'current' | 'minimal'>(() => new URLSearchParams(window.location.search).has('minimal') ? 'minimal' : 'current');
  const [selected, setSelected] = useState(2);
  const [query, setQuery] = useState('');
  const [leftWidth, setLeftWidth] = useState(260);
  const [rightWidth, setRightWidth] = useState(230);
  const [waveHeight, setWaveHeight] = useState(150);
  const drag = useRef<{ key: PanelKey; start: number; value: number } | null>(null);

  const resize = (key: PanelKey, e: React.PointerEvent) => {
    e.preventDefault();
    const start = key === 'waveform' ? e.clientY : e.clientX;
    const value = key === 'sidebar' ? leftWidth : key === 'info' ? rightWidth : waveHeight;
    drag.current = { key, start, value };
    const move = (event: PointerEvent) => {
      if (!drag.current) return;
      const delta = (key === 'waveform' ? drag.current.start - event.clientY : event.clientX - drag.current.start);
      if (key === 'sidebar') setLeftWidth(Math.max(190, Math.min(380, drag.current.value + delta)));
      if (key === 'info') setRightWidth(Math.max(180, Math.min(360, drag.current.value - delta)));
      if (key === 'waveform') setWaveHeight(Math.max(100, Math.min(300, drag.current.value + delta)));
    };
    const up = () => { drag.current = null; window.removeEventListener('pointermove', move); window.removeEventListener('pointerup', up); };
    window.addEventListener('pointermove', move); window.addEventListener('pointerup', up);
  };

  return <main className={`reference-shell reference-${mode}`}>
    <header className="reference-menu"><span className="reference-brand">Harken Search</span><nav><span>File</span><span>Edit</span><span>View</span><span>Database</span><span>Playlist</span><span>Tools</span><span>Settings</span><span>Help</span></nav><span className="reference-build">UI MIGRATION REFERENCE</span></header>
    <section className="reference-toolbar"><div className="history-buttons"><button>‹ <span>Back</span></button><button><span>Continue</span> ›</button></div><div className="reference-search-group"><div className="reference-search"><span>⌕</span><input value={query} onChange={(e) => setQuery(e.target.value)} placeholder="搜索 (多词用空格分隔)…" /></div><span className="reference-result-badge">{query ? 'Results' : 'No results'}</span></div><div className="reference-toolbar-tabs"><button>▣ &nbsp;File Browser</button><button className="is-on">◉ &nbsp;Database</button><button>☷ &nbsp;Playlists</button><button>⌕ &nbsp;Scan</button><button>↻ &nbsp;Refresh</button><button className="toolbar-more">⋯</button></div><div className="reference-audio-types"><button className="is-on">SFX</button><button>Music</button></div></section>
    <section className="reference-workspace" style={{ gridTemplateColumns: `${leftWidth}px minmax(0, 1fr) ${rightWidth}px` }}>
      <aside className="reference-sidebar"><div className="reference-section-head"><span>Filter</span><button>⋯</button></div><div className="reference-sidebar-block"><div className="reference-label">Related Paths <small>· H:\Audio Library</small></div><div className="reference-path-list">{folders.slice(0, 5).map((folder) => <button key={folder}>{folder}</button>)}</div></div><div className="reference-sidebar-block reference-tree-block"><div className="reference-label">System Folder Tree <small>· H:\Audio Library</small></div><div className="reference-tree">{folders.map((folder, index) => <button key={folder} className={index === 0 ? 'tree-open' : ''}><span>{index < 4 ? '›' : '·'}</span>{folder}<em>{index % 3 === 0 ? 12 + index : ''}</em></button>)}</div></div><div className="reference-sidebar-block"><div className="reference-label">Metadata</div><div className="reference-filter-row"><input placeholder="Artist" /><input placeholder="Category" /></div><button className="reference-apply">Apply filters</button></div><Grip direction="vertical" onPointerDown={(e) => resize('sidebar', e)} /></aside>
      <section className="reference-results"><div className="reference-results-head"><div><span className="eyebrow">DATABASE</span><strong>Sound effects library</strong></div><span className="result-count">3,822 files</span></div><div className="reference-breadcrumb">H: / Sound effect / Impacts / Punches</div><div className="reference-list-head"><span>Name</span><span>Waveform</span><span>Duration</span><span>Format</span><span>Rate</span><span>Category</span></div><div className="reference-list">{rows.filter((row) => row[0].toLowerCase().includes(query.toLowerCase())).map((row, index) => <button key={row[0]} className={`reference-row ${selected === index ? 'selected' : ''}`} onClick={() => setSelected(index)}><span>{row[0]}</span><span className="mini-wave">{Array.from({ length: 24 }, (_, i) => <i key={i} style={{ height: `${18 + ((i * (index + 3) * 13) % 70)}%` }} />)}</span><span>{row[1]}</span><span>{row[2]}</span><span>{row[3]}</span><span>{row[4]}</span></button>)}</div><div className="reference-results-foot"><span>Showing {rows.length} of 3,822</span><span>↑ Click a row to audition</span></div><Grip direction="horizontal" onPointerDown={(e) => resize('waveform', e)} /></section>
      <aside className="reference-info"><div className="reference-section-head"><span>Info</span><button>⋯</button></div><div className="info-tabs"><button className="is-on">Metadata</button><button>Artwork</button><button>Transfer</button><button>Play</button></div><div className="cover-placeholder">No cover in current library</div><div className="info-details"><span>Selected file</span><strong>{rows[selected]?.[0] ?? '—'}</strong><dl><dt>Duration</dt><dd>{rows[selected]?.[1]}</dd><dt>Format</dt><dd>WAV · 48 kHz</dd><dt>Channels</dt><dd>2 stereo</dd><dt>Category</dt><dd>Sound effect</dd></dl></div><Grip direction="vertical" onPointerDown={(e) => resize('info', e)} /></aside>
    </section>
    <footer className="reference-transport" style={{ height: waveHeight }}><div className="transport-controls"><strong>00:00:00.11</strong><button className="transport-play">▶</button><button>■</button><span className="transport-progress"><i /></span><span>38%</span><button>∞</button><span className="transport-status">NORM</span><button>Loop</button><span className="transport-spacer" /><span>Send to REAPER</span></div><Waveform /><div className="transport-ruler"><span>00:00:00.00</span><span>00:00:04</span><span>00:00:08</span><span>00:00:12</span></div></footer>
  </main>;
}

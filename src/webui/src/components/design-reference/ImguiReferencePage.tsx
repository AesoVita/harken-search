import React, { useState } from 'react';
import './imgui-reference.css';

const menus = ['File', 'Edit', 'View', 'Database', 'Playlist', 'Tools', 'Settings', 'Help'];
const nav = ['File Browser', 'Database', 'Playlists', 'Scan', 'Refresh'];

function DrawIcon({ type }: { type: string }) {
  const common = { viewBox: '0 0 18 18', fill: 'none', stroke: 'currentColor', strokeWidth: 1.35, strokeLinecap: 'round' as const, strokeLinejoin: 'round' as const };
  if (type === 'back') return <svg {...common}><path d="M11.5 4.5 7 9l4.5 4.5" /></svg>;
  if (type === 'forward') return <svg {...common}><path d="m6.5 4.5 4.5 4.5-4.5 4.5" /></svg>;
  if (type === 'browser') return <svg {...common}><path d="M3 6.2h4l1.4-1.7H15v9.7H3z" /><path d="M3 6.2h12" /></svg>;
  if (type === 'database') return <svg {...common}><ellipse cx="9" cy="4.5" rx="5.6" ry="2.2" /><path d="M3.4 4.5v8.5c0 1.2 2.5 2.3 5.6 2.3s5.6-1.1 5.6-2.3V4.5" /><path d="M3.4 8.6c0 1.2 2.5 2.2 5.6 2.2s5.6-1 5.6-2.2" /></svg>;
  if (type === 'playlists') return <svg {...common}><path d="M4 4.5h1M8 4.5h6M4 8.8h1M8 8.8h6M4 13h1M8 13h6" /></svg>;
  if (type === 'scan') return <svg {...common}><circle cx="7.7" cy="7.7" r="4.7" /><path d="m11.2 11.2 3.4 3.4M5.6 7.7h4.2M7.7 5.6v4.2" /></svg>;
  if (type === 'refresh') return <svg {...common}><path d="M14 6.5A5.5 5.5 0 1 0 14.2 11" /><path d="M14 3.8v3.5h-3.5" /></svg>;
  return <svg {...common}><circle cx="9" cy="9" r="6" /></svg>;
}

export default function ImguiReferencePage() {
  const [query, setQuery] = useState('');
  return <main className="imgui-ref">
    <div className="imgui-menu" role="menubar">
      {menus.map((item) => <button key={item}>{item}</button>)}
    </div>
    <div className="imgui-toolbar">
      <div className="imgui-history">
        <button className="history-back disabled" aria-disabled="true"><DrawIcon type="back" /><span>Back</span></button>
        <button className="history-forward disabled" aria-disabled="true"><span>Continue</span><DrawIcon type="forward" /></button>
      </div>
      <div className="imgui-search-group">
        <button className="search-action" aria-label="Search"><span /></button>
        <input value={query} onChange={(e) => setQuery(e.target.value)} placeholder="搜索（多个词用空格分隔）..." />
      </div>
      <span className="imgui-status">{query ? '...' : 'No results'}</span>
      <div className="imgui-nav">
        {nav.map((label, index) => <button key={label} className={index === 1 ? 'active' : ''}><DrawIcon type={['browser', 'database', 'playlists', 'scan', 'refresh'][index]} /><span>{label}</span></button>)}
        <i className="nav-divider" />
        <button className="imgui-dice" aria-label="Shuffle"><span className="dice-face"><i /><i /><i /><i /></span></button>
      </div>
      <div className="imgui-mode"><button className="active">SFX</button><button>Music</button></div>
    </div>
    <div className="imgui-body"><span>No results</span></div>
  </main>;
}

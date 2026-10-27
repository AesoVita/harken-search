import React, { useEffect, useState } from 'react';
import RuntimePresetDial from './RuntimePresetDial';

type Props = {
  style: React.CSSProperties;
  query: string;
  onQueryChange: (query: string) => void;
  canGoBack: boolean;
  canGoForward: boolean;
  onBack: () => void;
  onForward: () => void;
};
const menus = ['File', 'Edit', 'View', 'Database', 'Playlist', 'Tools', 'Settings', 'Help'];
const modes = [['folder', 'File Browser', true], ['database', 'Database', true], ['list', 'Playlists', true], ['search', 'Scan', false], ['refresh', 'Refresh', false]] as const;
type ToolbarGlyphName = typeof modes[number][0];
const DICE_ANIMATION_DURATION = 0.3;
const DICE_FACE_INTERVAL = 0.05;
const DICE_SHAKE_FREQUENCY = 50;

function ToolbarGlyph({ name }: { name: ToolbarGlyphName }) {
  const paths: Record<string, React.ReactNode> = {
    folder: <path d="M2 5.5V14h14V5.5h-6L7.7 2H2v3.5Z" />,
    database: <><ellipse cx="9" cy="3.5" rx="6" ry="2.5" /><path d="M3 3.5v12M15 3.5v12" /><ellipse cx="9" cy="9.5" rx="6" ry="2.5" /><ellipse cx="9" cy="15.5" rx="6" ry="2.5" /></>,
    list: <><circle cx="3" cy="4" r="1.1" fill="currentColor" stroke="none" /><circle cx="3" cy="9" r="1.1" fill="currentColor" stroke="none" /><circle cx="3" cy="14" r="1.1" fill="currentColor" stroke="none" /><path d="M6.5 4H15M6.5 9H15M6.5 14H15" /></>,
    search: <><circle cx="8" cy="8" r="5" /><path d="m11.5 11.5 4 4M5.5 8h5M8 5.5v5" /></>,
    refresh: <><path d="M14.5 8.8A6 6 0 1 1 12 3.9" /><path d="m11.4 1.5.6 3.7 3.6-.9" /></>,
  };
  return <svg className="glyph" viewBox="0 0 18 18" aria-hidden="true" fill="none" stroke="currentColor" strokeWidth="1.4" strokeLinecap="round" strokeLinejoin="round">{paths[name]}</svg>;
}

function DiceGlyph({ face, shake }: { face: number; shake: number }) {
  const dots: Record<number, Array<[number, number]>> = {
    1: [[9, 9]],
    2: [[5, 5], [13, 13]],
    3: [[5, 5], [9, 9], [13, 13]],
    4: [[5, 5], [13, 5], [5, 13], [13, 13]],
    5: [[5, 5], [13, 5], [9, 9], [5, 13], [13, 13]],
    6: [[5, 5], [13, 5], [5, 9], [13, 9], [5, 13], [13, 13]],
  };
  return <svg className="glyph dice-glyph" style={{ transform: `translateX(${shake}px)` }} viewBox="0 0 18 18" aria-hidden="true" fill="none" stroke="currentColor" strokeWidth="1.2">
    <rect x="1" y="1" width="16" height="16" rx="3.5" />
    {dots[face].map(([cx, cy], index) => <circle key={index} cx={cx} cy={cy} r="2" fill="currentColor" stroke="none" />)}
  </svg>;
}

function HistoryChevron({ direction }: { direction: 'back' | 'forward' }) {
  const points = direction === 'back' ? '11 4 6 9 11 14' : '7 4 12 9 7 14';
  return <svg className="history-chevron" viewBox="0 0 18 18" aria-hidden="true"><polyline points={points} /></svg>;
}

export default function RuntimeChrome({ style, query, onQueryChange, canGoBack, canGoForward, onBack, onForward }: Props) {
  const [activeNav, setActiveNav] = useState(0);
  const [diceRun, setDiceRun] = useState(0);
  const [diceFace, setDiceFace] = useState(1);
  const [diceShake, setDiceShake] = useState(0);

  useEffect(() => {
    if (diceRun === 0) return undefined;
    let frame = 0;
    let lastFaceAt = 0;
    const startedAt = performance.now();
    const animate = (now: number) => {
      const elapsed = (now - startedAt) / 1000;
      if (elapsed >= DICE_ANIMATION_DURATION) {
        setDiceFace(1 + Math.floor(Math.random() * 6));
        setDiceShake(0);
        return;
      }
      if (elapsed - lastFaceAt >= DICE_FACE_INTERVAL) {
        lastFaceAt = elapsed;
        setDiceFace(1 + Math.floor(Math.random() * 6));
      }
      setDiceShake(Math.sin(elapsed * DICE_SHAKE_FREQUENCY) * 3 * Math.max(0, 1 - elapsed * 3));
      frame = requestAnimationFrame(animate);
    };
    frame = requestAnimationFrame(animate);
    return () => cancelAnimationFrame(frame);
  }, [diceRun]);

  const handleDiceClick = () => setDiceRun(run => run + 1);

  return <header className="runtime-region runtime-chrome" style={style} data-source="src/main.cpp: RenderUI menu and toolbar">
    <div className="runtime-title"><span className="app-mark">A</span><span>Aris - JUSTMIXRFull (0 文件)</span><span className="window-controls">—　□　×</span></div>
    <nav className="runtime-menu">{menus.map(x => <span key={x}>{x}</span>)}</nav>
    <div className="runtime-toolbar">
      <div className="history-capsule" data-source="src/main.cpp:42180-42335"><button className={`nav-back ${canGoBack ? '' : 'disabled'}`} aria-label="Back" disabled={!canGoBack} onClick={onBack}><HistoryChevron direction="back" /><span>Back</span></button><i className="history-divider" aria-hidden="true" /><button className={`nav-forward ${canGoForward ? '' : 'disabled'}`} aria-label="Continue" disabled={!canGoForward} onClick={onForward}><span>Continue</span><HistoryChevron direction="forward" /></button></div>
      <span className="search-icon"><ToolbarGlyph name="search" /></span><input className="search-box" value={query} onChange={event => onQueryChange(event.target.value)} placeholder="搜索（多词用空格分隔）..." aria-label="Search library" />
      <div className="toolbar-nav-flow" data-source="src/main.cpp: ResolveToolbarFlowLayout">
        <div className="toolbar-primary-nav top-modes" data-source="src/main.cpp: toolbarFlow.itemSlots[0..2]">{modes.slice(0, 3).map(([icon, name, isModeButton], index) => <button className={isModeButton && index === activeNav ? 'active' : ''} aria-pressed={index === activeNav} onClick={() => setActiveNav(index)} key={name}><i><ToolbarGlyph name={icon} /></i>{name}</button>)}</div>
        <div className="preset-dial-slot" data-source="src/main.cpp: toolbarFlow.presetDial"><RuntimePresetDial /></div>
        <div className="toolbar-secondary-actions" data-source="src/main.cpp: toolbarFlow.itemSlots[3..5]">
          {modes.slice(3).map(([icon, name]) => <button key={name} aria-label={name}><i><ToolbarGlyph name={icon} /></i><span>{name}</span></button>)}
          <i className="nav-dice-divider" aria-hidden="true" />
          <button className="nav-dice" aria-label="Random file" onClick={handleDiceClick}><i><DiceGlyph face={diceFace} shake={diceShake} /></i></button>
        </div>
      </div>
    </div>
  </header>;
}

import React, { useEffect, useState } from 'react';
import { ARIS_RUNTIME_REFERENCE } from './runtime-contract';
import RuntimeChrome from './RuntimeChrome';
import RuntimeInfoPanel from './RuntimeInfoPanel';
import RuntimeSidebar from './RuntimeSidebar';
import RuntimeTransport from './RuntimeTransport';
import RuntimeWorkspace from './RuntimeWorkspace';
import './runtime-twin.css';

const LEFT_SIDEBAR_MIN_WIDTH = 160;
const RIGHT_PANEL_MIN_WIDTH = 150;
const MIN_READABLE_CENTER_WIDTH = 520;
const INITIAL_BROWSER_PATH = 'D:\\Valorant-WAV\\pakchunk0-WindowsClient';

const regionStyle = (name: keyof typeof ARIS_RUNTIME_REFERENCE.regions, override: Partial<React.CSSProperties> = {}) => {
  const { rect } = ARIS_RUNTIME_REFERENCE.regions[name];
  return { left: rect.x, top: rect.y, width: rect.width, height: rect.height, ...override };
};

export default function RuntimeTwinPage() {
  const splitterWidth = 12; // src/main.cpp:30291 and 43740
  const [scale, setScale] = useState(1);
  const [sidebarWidth, setSidebarWidth] = useState(516);
  const [infoWidth, setInfoWidth] = useState(530);
  const [globalQuery, setGlobalQuery] = useState('');
  const [browserHistory, setBrowserHistory] = useState([INITIAL_BROWSER_PATH]);
  const [browserHistoryIndex, setBrowserHistoryIndex] = useState(0);
  const [activeSplitter, setActiveSplitter] = useState<'left' | 'right' | null>(null);
  useEffect(() => {
    const resize = () => setScale(Math.min(
      window.innerWidth / ARIS_RUNTIME_REFERENCE.width,
      window.innerHeight / ARIS_RUNTIME_REFERENCE.height,
    ));
    resize();
    window.addEventListener('resize', resize);
    return () => window.removeEventListener('resize', resize);
  }, []);

  const startSidebarResize = (event: React.PointerEvent<HTMLSpanElement>) => {
    event.preventDefault();
    setActiveSplitter('left');
    const startX = event.clientX;
    const startWidth = sidebarWidth;
    const maxWidth = Math.max(400, ARIS_RUNTIME_REFERENCE.width - infoWidth - splitterWidth * 2 - MIN_READABLE_CENTER_WIDTH);
    const move = (moveEvent: PointerEvent) => setSidebarWidth(Math.max(LEFT_SIDEBAR_MIN_WIDTH, Math.min(maxWidth, startWidth + (moveEvent.clientX - startX) / scale)));
    const stop = () => { setActiveSplitter(null); window.removeEventListener('pointermove', move); window.removeEventListener('pointerup', stop); };
    window.addEventListener('pointermove', move);
    window.addEventListener('pointerup', stop, { once: true });
  };

  const startInfoResize = (event: React.PointerEvent<HTMLSpanElement>) => {
    event.preventDefault();
    setActiveSplitter('right');
    const startX = event.clientX;
    const startWidth = infoWidth;
    const maxWidth = Math.max(260, ARIS_RUNTIME_REFERENCE.width - sidebarWidth - splitterWidth * 2 - MIN_READABLE_CENTER_WIDTH);
    const move = (moveEvent: PointerEvent) => setInfoWidth(Math.max(RIGHT_PANEL_MIN_WIDTH, Math.min(maxWidth, startWidth - (moveEvent.clientX - startX) / scale)));
    const stop = () => { setActiveSplitter(null); window.removeEventListener('pointermove', move); window.removeEventListener('pointerup', stop); };
    window.addEventListener('pointermove', move);
    window.addEventListener('pointerup', stop, { once: true });
  };

  const workspaceX = 8 + sidebarWidth + splitterWidth;
  const infoX = ARIS_RUNTIME_REFERENCE.width - 8 - infoWidth;
  const workspaceWidth = infoX - workspaceX - splitterWidth;
  const navigateBrowserPath = (path: string) => {
    const target = path.trim();
    if (!target || target === browserHistory[browserHistoryIndex]) return;
    setBrowserHistory(current => [...current.slice(0, browserHistoryIndex + 1), target]);
    setBrowserHistoryIndex(browserHistoryIndex + 1);
  };
  const goBrowserBack = () => setBrowserHistoryIndex(index => Math.max(0, index - 1));
  const goBrowserForward = () => setBrowserHistoryIndex(index => Math.min(browserHistory.length - 1, index + 1));

  return <main className="runtime-twin">
    <div className="runtime-stage" style={{ transform: `translate(-50%,-50%) scale(${scale})` }} aria-label="Harken Search runtime web twin">
      <RuntimeChrome style={regionStyle('chrome')} query={globalQuery} onQueryChange={setGlobalQuery} canGoBack={browserHistoryIndex > 0} canGoForward={browserHistoryIndex < browserHistory.length - 1} onBack={goBrowserBack} onForward={goBrowserForward} />
      <RuntimeSidebar style={regionStyle('sidebar', { width: sidebarWidth })} />
      <span className={`runtime-splitter left ${activeSplitter === 'left' ? 'active' : ''}`} style={{ left: 8 + sidebarWidth, top: ARIS_RUNTIME_REFERENCE.regions.sidebar.rect.y, height: ARIS_RUNTIME_REFERENCE.regions.sidebar.rect.height }} role="separator" tabIndex={0} aria-label="Resize filter panel" onPointerDown={startSidebarResize}><i /><i /><i /></span>
      <RuntimeWorkspace style={regionStyle('workspace', { left: workspaceX, width: workspaceWidth })} globalQuery={globalQuery} committedBrowserPath={browserHistory[browserHistoryIndex]} onNavigateBrowserPath={navigateBrowserPath} />
      <span className={`runtime-splitter right ${activeSplitter === 'right' ? 'active' : ''}`} style={{ left: infoX - splitterWidth, top: ARIS_RUNTIME_REFERENCE.regions.info.rect.y, height: ARIS_RUNTIME_REFERENCE.regions.info.rect.height }} role="separator" tabIndex={0} aria-label="Resize info panel" onPointerDown={startInfoResize}><i /><i /><i /></span>
      <RuntimeInfoPanel style={regionStyle('info', { left: infoX, width: infoWidth })} />
      <RuntimeTransport style={regionStyle('transport')} />
    </div>
  </main>;
}

import React, { useCallback, useEffect, useId, useMemo, useRef, useState } from 'react';
import { invoke, onEvent } from './bridge';
import type { AppSettingsDto, AudioRecord, FileCoverResponse, FileInfoDetails, FolderNode, PlaylistItem, SearchFilters, SearchQueryResponse, TransportState } from './types';
import { useDebouncedValue } from './hooks/useDebouncedValue';
import MetadataFilters from './components/filters/MetadataFilters';
import type { FilterOptions, FilterState } from './components/filters/types';
import MiniWaveform from './components/waveform/MiniWaveform';
import MenuBar from './components/menu/MenuBar';
import type { RecentDbInfo } from './components/menu/MenuBar';

type ConfirmDialogState = {
  title: string;
  message: string;
  okText?: string;
  cancelText?: string;
  resolve: (ok: boolean) => void;
} | null;

type PromptDialogState = {
  title: string;
  label: string;
  initial?: string;
  placeholder?: string;
  okText?: string;
  cancelText?: string;
  resolve: (val: string | null) => void;
} | null;

const formatDuration = (seconds: number) => {
  if (!seconds || seconds <= 0) return '0:00';
  const m = Math.floor(seconds / 60);
  const s = Math.floor(seconds % 60);
  return `${m}:${String(s).padStart(2, '0')}`;
};

const pad2 = (n: number) => String(Math.max(0, Math.floor(n))).padStart(2, '0');
const pad3 = (n: number) => String(Math.max(0, Math.floor(n))).padStart(3, '0');

function formatTimecode(seconds: number, s: AppSettingsDto) {
  const sec = Math.max(0, seconds || 0);
  const h = Math.floor(sec / 3600);
  const m = Math.floor(sec / 60) % 60;
  const ss = Math.floor(sec) % 60;
  const frac = sec - Math.floor(sec);
  const showMs = s.showMilliseconds !== false;
  const showFrames = !!s.showFrameNumber;
  const fr = Math.max(1, Math.min(120, s.defaultFrameRate ?? 25));

  if (showFrames) {
    const frameInSec = Math.floor(frac * fr) % Math.floor(fr);
    return `${pad2(h)}:${pad2(m)}:${pad2(ss)}:${pad2(frameInSec)}`;
  }
  if (showMs) {
    const ms = Math.floor(frac * 1000) % 1000;
    return `${pad2(h)}:${pad2(m)}:${pad2(ss)}.${pad3(ms)}`;
  }
  return `${pad2(h)}:${pad2(m)}:${pad2(ss)}`;
}

function sameTransportState(left: TransportState, right: TransportState): boolean {
  return left.playing === right.playing &&
    left.loaded === right.loaded &&
    left.position === right.position &&
    left.duration === right.duration &&
    left.pitch === right.pitch &&
    left.volume === right.volume &&
    left.inPoint === right.inPoint &&
    left.outPoint === right.outPoint &&
    left.fileId === right.fileId &&
    left.reversed === right.reversed;
}

function FolderIcon({ open }: { open: boolean }) {
  // Premium folder icon: layered gradients + subtle outline/highlight (more "native app", less flat)
  const gid = useId();
  const gBack = `${gid}-fb`;
  const gFront = `${gid}-ff`;
  const gLip = `${gid}-fl`;
  const outline = 'rgba(0,0,0,0.28)';

  return open ? (
    <svg className="f-tree-folder-ico" width="16" height="14" viewBox="0 0 20 16" aria-hidden="true">
      <defs>
        <linearGradient id={gBack} x1="0" y1="0" x2="0" y2="16">
          <stop offset="0" stopColor="#FDE68A" />
          <stop offset="1" stopColor="#F59E0B" />
        </linearGradient>
        <linearGradient id={gFront} x1="0" y1="4" x2="0" y2="16">
          <stop offset="0" stopColor="#FBBF24" />
          <stop offset="1" stopColor="#D97706" />
        </linearGradient>
        <linearGradient id={gLip} x1="0" y1="4" x2="20" y2="10">
          <stop offset="0" stopColor="rgba(255,255,255,0.55)" />
          <stop offset="1" stopColor="rgba(255,255,255,0)" />
        </linearGradient>
      </defs>

      {/* Back/top flap */}
      <path
        d="M1.6 5.2c0-1 .8-1.8 1.8-1.8h5.0c.5 0 1 .2 1.3.6l1.0 1H16.6c1 0 1.8.8 1.8 1.8v.7H1.6v-1z"
        fill={`url(#${gBack})`}
        stroke={outline}
        strokeWidth="0.8"
      />

      {/* Front body (open perspective) */}
      <path
        d="M2.0 6.0h16.5c.9 0 1.5.7 1.3 1.6l-1.0 6.0c-.2 1-1.0 1.7-2.0 1.7H3.7c-1.1 0-2.0-.9-2.1-2.0L1.2 7.7C1.1 6.8 1.5 6.0 2.0 6.0z"
        fill={`url(#${gFront})`}
        stroke={outline}
        strokeWidth="0.8"
      />

      {/* Lip highlight */}
      <path d="M2.2 6.3h15.8" stroke={`url(#${gLip})`} strokeWidth="1.0" opacity="0.9" />

      {/* Specular highlight */}
      <path
        d="M3.2 7.2c.2-.6.7-1 1.3-1h4.0"
        stroke="rgba(255,255,255,0.35)"
        strokeWidth="1"
        strokeLinecap="round"
      />
    </svg>
  ) : (
    <svg className="f-tree-folder-ico" width="16" height="14" viewBox="0 0 20 16" aria-hidden="true">
      <defs>
        <linearGradient id={gBack} x1="0" y1="0" x2="0" y2="16">
          <stop offset="0" stopColor="#FDE68A" />
          <stop offset="1" stopColor="#F59E0B" />
        </linearGradient>
        <linearGradient id={gFront} x1="0" y1="3" x2="0" y2="16">
          <stop offset="0" stopColor="#FBBF24" />
          <stop offset="1" stopColor="#D97706" />
        </linearGradient>
        <linearGradient id={gLip} x1="0" y1="5" x2="20" y2="10">
          <stop offset="0" stopColor="rgba(255,255,255,0.45)" />
          <stop offset="1" stopColor="rgba(255,255,255,0)" />
        </linearGradient>
      </defs>

      {/* Back/top */}
      <path
        d="M2.0 4.1c0-1 .8-1.8 1.8-1.8h5.2c.5 0 1 .2 1.3.6l.9 1H16.2c1 0 1.8.8 1.8 1.8v.8H2.0V4.1z"
        fill={`url(#${gBack})`}
        stroke={outline}
        strokeWidth="0.8"
      />

      {/* Front */}
      <path
        d="M2.0 6.2h16.0v6.2c0 1.1-.9 2-2 2H4.0c-1.1 0-2-.9-2-2V6.2z"
        fill={`url(#${gFront})`}
        stroke={outline}
        strokeWidth="0.8"
      />

      <path d="M2.1 6.3h15.8" stroke={`url(#${gLip})`} strokeWidth="1.0" opacity="0.85" />
      <path d="M3.0 3.8h5.3" stroke="rgba(255,255,255,0.22)" strokeWidth="1" strokeLinecap="round" />
    </svg>
  );
}

function FolderTree({
  node,
  onClick,
  selectedPath,
}: {
  node: FolderNode;
  onClick: (path: string) => void;
  selectedPath: string;
}) {
  const [open, setOpen] = useState(true);
  const hasChildren = !!node.subfolders?.length;
  const isSelected = !!selectedPath && selectedPath === node.path;
  return (
    <div className="f-tree-node">
      <button
        className={`f-tree-line${isSelected ? ' f-tree-line-on' : ''}`}
        onClick={() => {
          setOpen((v) => !v);
          onClick(node.path);
        }}
      >
        <span className="f-arrow">{hasChildren ? (open ? '▾' : '▸') : ''}</span>
        <span className="f-tree-ico">
          <FolderIcon open={open && hasChildren} />
        </span>
        <span className="f-name">{node.name || node.path || 'Root'}</span>
        {!!node.fileCount && <span className="f-count">{node.fileCount}</span>}
      </button>
      {open && hasChildren && (
        <div className="f-tree-children">
          {node.subfolders.map((s) => (
            <FolderTree key={s.path} node={s} onClick={onClick} selectedPath={selectedPath} />
          ))}
        </div>
      )}
    </div>
  );
}

export default function App() {
  const defaultFilterState: FilterState = {
    artist: [],
    album: [],
    category: [],
    tags: [],
    format: [],
    sampleRate: [],
    channels: 'any',
    duration: {},
  };
  const [query, setQuery] = useState('');
  const [committedQuery, setCommittedQuery] = useState('');
  const [rows, setRows] = useState<AudioRecord[]>([]);
  const [selectedId, setSelectedId] = useState<number | null>(null);
  const selectedIdRef = useRef<number | null>(null);
  useEffect(() => {
    selectedIdRef.current = selectedId;
  }, [selectedId]);
  const [selectedIds, setSelectedIds] = useState<Set<number>>(new Set());
  const lastClickedIndexRef = useRef<number>(-1);
  const listDragRef = useRef<{ id: number; x: number; y: number; didDrag: boolean; pointerId: number } | null>(null);
  const suppressRowClickUntilRef = useRef<number>(0);
  const gridListRef = useRef<HTMLDivElement | null>(null);
  const wheelSaveTimerRef = useRef<number | null>(null);
  const latestSettingsRef = useRef<AppSettingsDto>({
    playbackMode: 0, searchRealtime: true, rowHeight: 32, fontSize: 12,
    waveformPanelHeight: 200, showFileExtension: false, useWebUIMain: true,
  });
  const wheelPendingSaveRef = useRef<{ rowHeight: number; fontSize: number } | null>(null);
  const [details, setDetails] = useState<FileInfoDetails | null>(null);
  const [cover, setCover] = useState<FileCoverResponse | null>(null);
  const [showCoverPreview, setShowCoverPreview] = useState(false);
  const [folderTree, setFolderTree] = useState<FolderNode | null>(null);
  const [status, setStatus] = useState('Ready');
  const [loading, setLoading] = useState(false);
  const [total, setTotal] = useState(0);
  const totalRef = useRef(0);
  useEffect(() => { totalRef.current = total; }, [total]);
  const [sortColumn, setSortColumn] = useState<'fileName' | 'duration' | 'sampleRate' | 'format'>('fileName');
  const [sortAsc, setSortAsc] = useState(true);
  const [isPlaying, setIsPlaying] = useState(false);
  const [currentTime, setCurrentTime] = useState(0);
  const [volume, setVolume] = useState(1.0);
  const [pitch, setPitch] = useState(0);
  const transportAdjustRef = useRef<{ pitch: boolean; volume: boolean }>({ pitch: false, volume: false });
  const [transport, setTransport] = useState<TransportState>({
    playing: false,
    loaded: false,
    position: 0,
    duration: 0,
    pitch: 0,
    volume: 1,
    inPoint: 0,
    outPoint: 1,
  });
  const transportPlayingRef = useRef(false);
  const transportFileIdRef = useRef<number | null>(null);
  useEffect(() => {
    transportPlayingRef.current = !!transport.playing;
    transportFileIdRef.current = transport.fileId ?? null;
  }, [transport.fileId, transport.playing]);
  // NOTE: 主波形/光谱交互已切回 ImGui 原生面板（混合模式），WebUI 不再渲染 MainWaveform。
  const [contextMenu, setContextMenu] = useState<{ x: number; y: number; row: AudioRecord } | null>(null);
  const [columnMenu, setColumnMenu] = useState<{ x: number; y: number } | null>(null);
  const [filterState, setFilterState] = useState<FilterState>(defaultFilterState);
  const [folderFilterPath, setFolderFilterPath] = useState('');
  const debouncedQuery = useDebouncedValue(query, 200);
  const [playerSelectionLink, setPlayerSelectionLink] = useState(true);
  // NOTE: 不再自动把选中行 scrollIntoView 居中（用户不需要“每次点击都自动定位到中间”）
  const [showSpectrogram, setShowSpectrogram] = useState(false);
  const [spectrogramSplit, setSpectrogramSplit] = useState(false);
  const [spectrogramBlend, setSpectrogramBlend] = useState(0.5);
  const [spectrogramFFTSize, setSpectrogramFFTSize] = useState(2048);
  const spectrogramSaveTimerRef = useRef<number | null>(null);
  // Cover filters (match ImGui): 0=Any, 1=Has cover, 2=No cover; plus optional same-cover key filter
  const [coverFilter, setCoverFilter] = useState<0 | 1 | 2>(0);
  const [specificCoverKey, setSpecificCoverKey] = useState('');
  const reloadPageRef = useRef<() => Promise<void>>(async () => {});
  const lastScanRefreshMsRef = useRef(0);
  const lastScanSigRef = useRef('');
  const wasScanningRef = useRef(false);
  // MenuBar state
  const [recentDbs, setRecentDbs] = useState<RecentDbInfo>({ current: '', items: [] });
  const [playlists, setPlaylists] = useState<PlaylistItem[]>([]);
  const [appSettings, setAppSettings] = useState<AppSettingsDto>({
    playbackMode: 0, searchRealtime: true, rowHeight: 32, fontSize: 12,
    waveformPanelHeight: 200, showFileExtension: false, useWebUIMain: true,
  });
  // Cover thumbnails for list (lazy, deduped). Keyed by fileId; value is dataUrl or null (known-missing/too-large).
  const coverThumbByIdRef = useRef<Map<number, string | null>>(new Map());
  const coverThumbByKeyRef = useRef<Map<string, string>>(new Map());
  const coverKeyByIdRef = useRef<Map<number, string>>(new Map());
  const coverInflightRef = useRef<Set<number>>(new Set());
  const coverKeyInflightRef = useRef<Set<string>>(new Set());
  const [, setCoverThumbRev] = useState(0); // bump to re-render cover cells
  const coverThumbRafRef = useRef<number | null>(null);
  const [resultVisibleRange, setResultVisibleRange] = useState<{ start: number; end: number }>({ start: 0, end: 0 });
  const [coverDropActive, setCoverDropActive] = useState(false);
  const [settingsOpen, setSettingsOpen] = useState(false);
  const [engineConnected, setEngineConnected] = useState(false);
  const showStatusInTransport = useMemo(() => {
    const s = (status || '').trim();
    if (!s) return false;
    if (s === 'Ready') return false;
    if (s === 'Connected to Harken Search') return false;
    // Avoid permanent "connected" noise — Settings dialog already shows engine status.
    if (s.toLowerCase().includes('connected')) return false;
    return true;
  }, [status]);
  const [aboutInfo, setAboutInfo] = useState<{ version: string; build: string; fileCount: number; dbPath: string } | null>(null);
  const [editFilenameDialog, setEditFilenameDialog] = useState<{ id: number; name: string } | null>(null);
  const [newDbDialog, setNewDbDialog] = useState(false);
  const [newDbName, setNewDbName] = useState('soundmanager');
  const [confirmDialog, setConfirmDialog] = useState<ConfirmDialogState>(null);
  const [promptDialog, setPromptDialog] = useState<PromptDialogState>(null);
  const [promptText, setPromptText] = useState('');

  const confirmAsync = useCallback(
    (title: string, message: string, okText = '确定', cancelText = '取消') =>
      new Promise<boolean>((resolve) => setConfirmDialog({ title, message, okText, cancelText, resolve })),
    [],
  );

  const promptAsync = useCallback(
    (title: string, label: string, initial = '', placeholder = '', okText = '确定', cancelText = '取消') =>
      new Promise<string | null>((resolve) => {
        setPromptText(initial);
        setPromptDialog({ title, label, initial, placeholder, okText, cancelText, resolve });
      }),
    [],
  );
  const mockRows = useMemo<AudioRecord[]>(() => {
    return Array.from({ length: 28 }).map((_, i) => ({
      id: 100000 + i,
      fileName: `Sound_Effect_${String(i).padStart(4, '0')}.${i % 3 === 0 ? 'wav' : i % 3 === 1 ? 'mp3' : 'ogg'}`,
      filePath: '',
      folderPath: '',
      durationSeconds: 40 + (i * 11) % 220,
      sampleRate: i % 2 ? 48000 : 44100,
      format: i % 3 === 0 ? 'WAV' : i % 3 === 1 ? 'MP3' : 'OGG',
      extension: '',
      lufsIntegrated: -18 + (i % 8),
      rms: -22 + (i % 7),
      truePeak: -1 + ((i % 5) * 0.2),
      category: ['SFX', 'Foley', 'Voice', 'Music'][i % 4],
      artist: '',
      album: '',
      description: '',
      keywords: '',
      channels: 2,
      fileSize: 1024 * 1024 * (2 + i % 8),
      dateAdded: 0,
      fileMtime: 0,
      lastIndexed: 0,
      played: false,
      hasCover: false,
      totalFrames: 0,
      playCount: 0,
      bpm: 0,
      bitRate: 0,
      bitDepth: 0,
      trackIndex: 0,
      discNumber: 0,
      year: 0,
      iSampleRate: 0,
      lengthSec: 0,
      lufsMomentary: 0,
      lufsShortTerm: 0,
      peak: 0,
      crestFactor: 0,
    } as unknown as AudioRecord));
  }, []);

  // Metadata editor state
  const [editDescription, setEditDescription] = useState('');
  const [editKeywords, setEditKeywords] = useState('');
  const [editCategory, setEditCategory] = useState('');

  // Batch editor state
  const [batchApplyDescription, setBatchApplyDescription] = useState(false);
  const [batchApplyKeywords, setBatchApplyKeywords] = useState(false);
  const [batchApplyCategory, setBatchApplyCategory] = useState(false);
  const [batchDescription, setBatchDescription] = useState('');
  const [batchKeywords, setBatchKeywords] = useState('');
  const [batchCategory, setBatchCategory] = useState('');

  const call = useCallback(async <T,>(method: string, params?: Record<string, unknown>): Promise<T | null> => {
    try {
      return await invoke<T>(method, params ?? {});
    } catch (e) {
      console.error(method, e);
      const msg = e instanceof Error ? e.message : String(e ?? '');
      setStatus(`RPC failed: ${method}${msg ? ` (${msg})` : ''}`);
      return null;
    }
  }, []);

  useEffect(() => {
    const boot = async () => {
      const ping = await call<{ pong?: boolean }>('ping');
      if (ping?.pong) {
        setEngineConnected(true);
        setStatus('Connected to Harken Search');
      } else {
        setEngineConnected(false);
      }
      const tree = await call<FolderNode>('sidebar.getFolderTree');
      if (tree) setFolderTree(tree);

      const settings = await call<AppSettingsDto>('menu.getSettings');
      if (settings && typeof settings.playerSelectionLink === 'boolean') {
        setPlayerSelectionLink(settings.playerSelectionLink);
      }
      if (settings && typeof settings.showSpectrogram === 'boolean') {
        setShowSpectrogram(settings.showSpectrogram);
      }
      if (settings && typeof settings.spectrogramSplit === 'boolean') {
        setSpectrogramSplit(settings.spectrogramSplit);
      }
      if (settings && typeof settings.spectrogramBlend === 'number') {
        setSpectrogramBlend(settings.spectrogramBlend);
      }
      if (settings && typeof settings.spectrogramFFTSize === 'number') {
        setSpectrogramFFTSize(settings.spectrogramFFTSize);
      }
      if (settings) {
        setAppSettings((prev) => ({ ...prev, ...settings }));
        if (settings.searchRealtime === false) {
          // When not realtime, keep last committed query stable.
          setCommittedQuery((v) => v || query);
        }
      }

      // Fetch recent databases for MenuBar
      const recent = await call<RecentDbInfo>('menu.getRecentDbs');
      if (recent) setRecentDbs(recent);

      // Fetch playlists for MenuBar
      const pls = await call<PlaylistItem[]>('sidebar.getPlaylists');
      if (pls) setPlaylists(pls);
    };
    boot();
  }, [call]);

  useEffect(() => {
    const preventDefaultDrop = (e: DragEvent) => {
      e.preventDefault();
    };
    window.addEventListener('dragover', preventDefaultDrop);
    window.addEventListener('drop', preventDefaultDrop);
    return () => {
      window.removeEventListener('dragover', preventDefaultDrop);
      window.removeEventListener('drop', preventDefaultDrop);
    };
  }, []);

  type GridCols = {
    cover: number;
    wave: number;
    duration: number;
    size: number;
    rate: number;
    fmt: number;
    category: number;
  };
  const defaultGridCols: GridCols = { cover: 34, wave: 140, duration: 92, size: 84, rate: 84, fmt: 62, category: 120 };

  // WebUI grid column widths (persisted in menu.saveSettings -> webUiLayout.column)
  const [gridCols, setGridCols] = useState<GridCols>(defaultGridCols);
  const gridColsRef = useRef(gridCols);
  useEffect(() => {
    gridColsRef.current = gridCols;
  }, [gridCols]);

  type ColKey = 'cover' | 'name' | 'wave' | 'duration' | 'size' | 'rate' | 'fmt' | 'category';
  const defaultColOrder: ColKey[] = ['cover', 'name', 'wave', 'duration', 'size', 'rate', 'fmt', 'category'];
  const [colOrder, setColOrder] = useState<ColKey[]>(defaultColOrder);
  const colOrderRef = useRef(colOrder);
  useEffect(() => {
    colOrderRef.current = colOrder;
  }, [colOrder]);
  const headerDragFromRef = useRef<ColKey | null>(null);

  const [hiddenCols, setHiddenCols] = useState<Set<ColKey>>(new Set());
  const hiddenColsRef = useRef(hiddenCols);
  useEffect(() => {
    hiddenColsRef.current = hiddenCols;
  }, [hiddenCols]);

  const visibleColOrder = useMemo(() => colOrder.filter((k) => k === 'name' || !hiddenCols.has(k)), [colOrder, hiddenCols]);
  const gridDragRef = useRef<{
    key: keyof typeof gridCols;
    startX: number;
    startW: number;
    lastW?: number;
  } | null>(null);

  useEffect(() => {
    const c = appSettings.webUiLayout?.column;
    if (!c) return;
    // Column order (persisted in column.order)
    const order = (c as any).order;
    if (Array.isArray(order) && order.length) {
      const next: ColKey[] = [];
      const seen = new Set<string>();
      for (const k of order) {
        if (typeof k !== 'string') continue;
        if (seen.has(k)) continue;
        if (k === 'cover' || k === 'name' || k === 'wave' || k === 'duration' || k === 'size' || k === 'rate' || k === 'fmt' || k === 'category') {
          next.push(k);
          seen.add(k);
        }
      }
      for (const k of defaultColOrder) if (!seen.has(k)) next.push(k);
      if (next.length) setColOrder(next);
    }
    const hidden = (c as any).hidden;
    if (Array.isArray(hidden)) {
      const s = new Set<ColKey>();
      for (const k of hidden) {
        if (typeof k !== 'string') continue;
        if (k === 'cover' || k === 'wave' || k === 'duration' || k === 'size' || k === 'rate' || k === 'fmt' || k === 'category') {
          s.add(k);
        }
      }
      setHiddenCols(s);
    }
    // tolerate missing keys
    setGridCols((prev) => ({
      ...prev,
      cover: typeof (c as any).cover === 'number' ? (c as any).cover : prev.cover,
      wave: typeof c.wave === 'number' ? c.wave : prev.wave,
      duration: typeof c.duration === 'number' ? c.duration : prev.duration,
      size: typeof c.size === 'number' ? c.size : prev.size,
      rate: typeof c.rate === 'number' ? c.rate : prev.rate,
      fmt: typeof c.fmt === 'number' ? c.fmt : prev.fmt,
      category: typeof c.category === 'number' ? c.category : prev.category,
    }));
  }, [appSettings.webUiLayout?.column]);

  const applyGridCols = useMemo(() => {
    // Build grid template based on column order; name is flexible, others are px-controlled.
    const parts: string[] = [];
    for (const k of visibleColOrder) {
      if (k === 'name') parts.push('minmax(320px, 1.8fr)');
      else if (k === 'cover') parts.push(`${gridCols.cover}px`);
      else if (k === 'wave') parts.push(`${gridCols.wave}px`);
      else if (k === 'duration') parts.push(`${gridCols.duration}px`);
      else if (k === 'size') parts.push(`${gridCols.size}px`);
      else if (k === 'rate') parts.push(`${gridCols.rate}px`);
      else if (k === 'fmt') parts.push(`${gridCols.fmt}px`);
      else if (k === 'category') parts.push(`${gridCols.category}px`);
    }
    return parts.join(' ');
  }, [gridCols, visibleColOrder]);

  const persistGridCols = useCallback(
    (next: typeof gridCols) => {
      setGridCols(next);
      void call('menu.saveSettings', { column: { ...next, order: colOrderRef.current, hidden: Array.from(hiddenColsRef.current) } as any });
    },
    [call],
  );

  const persistColumnOrder = useCallback(
    (nextOrder: ColKey[]) => {
      setColOrder(nextOrder);
      void call('menu.saveSettings', { column: { ...gridColsRef.current, order: nextOrder, hidden: Array.from(hiddenColsRef.current) } as any });
    },
    [call],
  );

  const toggleColumnHidden = useCallback(
    (k: ColKey) => {
      if (k === 'name') return; // keep name always visible
      setHiddenCols((prev) => {
        const next = new Set(prev);
        if (next.has(k)) next.delete(k);
        else next.add(k);
        // persist hidden list (order + widths remain)
        void call('menu.saveSettings', { column: { ...gridColsRef.current, order: colOrderRef.current, hidden: Array.from(next) } as any });
        return next;
      });
    },
    [call],
  );

  const resetColumnsToDefault = useCallback(() => {
    const nextCols: GridCols = { ...defaultGridCols };
    const nextOrder: ColKey[] = [...defaultColOrder];
    const nextHidden = new Set<ColKey>();
    gridColsRef.current = nextCols;
    colOrderRef.current = nextOrder;
    hiddenColsRef.current = nextHidden;
    setGridCols(nextCols);
    setColOrder(nextOrder);
    setHiddenCols(nextHidden);
    void call('menu.saveSettings', { column: { ...nextCols, order: nextOrder, hidden: [] } as any });
    setStatus('Columns reset to default');
  }, [call]);

  const autofitColumnsToContent = useCallback(() => {
    const sampleRows = rows.slice(0, Math.min(rows.length, 400));
    const canvas = document.createElement('canvas');
    const ctx = canvas.getContext('2d');
    if (!ctx) return;
    const bodyFontSize = Math.max(11, appSettings.fontSize ?? 12);
    ctx.font = `500 ${bodyFontSize}px "Segoe UI", "Microsoft YaHei UI", "PingFang SC", sans-serif`;

    const measure = (header: string, values: string[], minW: number, maxW: number) => {
      let maxText = ctx.measureText(header).width;
      for (const v of values) {
        const w = ctx.measureText(v).width;
        if (w > maxText) maxText = w;
      }
      // Add horizontal padding + a little safety for anti-aliasing and sort indicators.
      const w = Math.ceil(maxText + 26);
      return Math.max(minW, Math.min(maxW, w));
    };

    const durationValues = sampleRows.map((row) => formatDuration(row.durationSeconds ?? row.duration ?? 0));
    const sizeValues = sampleRows.map((row) => `${((row.fileSize || 0) / (1024 * 1024)).toFixed(1)} MB`);
    const rateValues = sampleRows.map((row) => String(row.sampleRate || '-'));
    const fmtValues = sampleRows.map((row) => String((row.format || row.extension || '-')).toUpperCase());

    const next: GridCols = {
      ...gridColsRef.current,
      duration: measure('DURATION', durationValues, 70, 220),
      size: measure('SIZE', sizeValues, 70, 240),
      rate: measure('RATE', rateValues, 70, 260),
      fmt: measure('FMT', fmtValues, 56, 200),
    };
    gridColsRef.current = next;
    setGridCols(next);
    void call('menu.saveSettings', {
      column: { ...next, order: colOrderRef.current, hidden: Array.from(hiddenColsRef.current) } as any,
    });
    setStatus('Columns auto-fitted to current content');
  }, [appSettings.fontSize, call, rows]);

  const getCoverThumbDataUrl = useCallback((id: number) => {
    // Use key cache when possible (multiple files may share the same folder/embedded cover).
    const byId = coverThumbByIdRef.current.get(id);
    return byId;
  }, []);

  const scheduleCoverThumbRefresh = useCallback(() => {
    if (coverThumbRafRef.current != null) return;
    coverThumbRafRef.current = window.requestAnimationFrame(() => {
      coverThumbRafRef.current = null;
      setCoverThumbRev((v) => v + 1);
    });
  }, []);

  const ensureCoverThumb = useCallback(
    async (id: number) => {
      const byId = coverThumbByIdRef.current;
      if (byId.has(id)) return;
      if (coverInflightRef.current.has(id)) return;
      coverInflightRef.current.add(id);
      try {
        // Fast path: if we already have a cached cover for this id/key, reuse it.
        const direct = coverThumbByKeyRef.current.get(`id:${id}`);
        if (direct) {
          byId.set(id, direct);
          return;
        }
        const c = await call<FileCoverResponse>('fileInfo.getCover', { id });
        if (!c) {
          byId.set(id, null);
          return;
        }
        if (c.cacheKey && c.dataUrl && typeof c.dataUrl === 'string') {
          coverThumbByKeyRef.current.set(c.cacheKey, c.dataUrl);
          coverThumbByKeyRef.current.set(`id:${id}`, c.dataUrl);
        }
        if (c.cacheKey) {
          coverKeyByIdRef.current.set(id, c.cacheKey);
        }
        // If cover is too large or missing, store null to avoid retry storms.
        const okUrl = c.dataUrl && c.dataUrl.startsWith('data:image/');
        byId.set(id, okUrl ? c.dataUrl : null);

        // Small cap to avoid unbounded memory growth.
        const kMax = 96;
        if (byId.size > kMax) {
          const it = byId.keys();
          while (byId.size > kMax) {
            const n = it.next();
            if (n.done) break;
            byId.delete(n.value);
          }
        }
        // Cap key cache as well (dataUrl can be large).
        const byKey = coverThumbByKeyRef.current;
        const kMaxKey = 96;
        if (byKey.size > kMaxKey) {
          const it = byKey.keys();
          while (byKey.size > kMaxKey) {
            const n = it.next();
            if (n.done) break;
            byKey.delete(n.value);
          }
        }
      } finally {
        coverInflightRef.current.delete(id);
        // Coalesce many cover updates into one paint to avoid "flashing" re-layout.
        scheduleCoverThumbRefresh();
      }
    },
    [call, scheduleCoverThumbRefresh],
  );

  const saveSettingsPatch = useCallback(
    (patch: Partial<AppSettingsDto> & { panel?: { left?: number; right?: number; bottom?: number } }) => {
      setAppSettings((prev) => ({ ...prev, ...patch }));
      void call('menu.saveSettings', patch as any);
      if (typeof (patch as any).playerSelectionLink === 'boolean') setPlayerSelectionLink(!!(patch as any).playerSelectionLink);
    },
    [call],
  );

  // Ctrl + Wheel: adjust WebUI density (rowHeight). Ctrl + Shift + Wheel: adjust fontSize.
  // Use a non-passive listener to prevent WebView2/Chromium default page zoom.
  useEffect(() => {
    // Keep latest settings for event handlers (avoid stale closure reads).
    latestSettingsRef.current = appSettings;
  }, [appSettings]);

  useEffect(() => {
    const onWheel = (e: WheelEvent) => {
      const isCtrl = e.ctrlKey || e.metaKey;
      if (!isCtrl) return;
      const target = e.target as Node | null;
      if (!gridListRef.current || !target || !gridListRef.current.contains(target)) return;

      e.preventDefault();
      e.stopPropagation();

      const dir = e.deltaY === 0 ? 0 : (e.deltaY > 0 ? 1 : -1);
      if (dir === 0) return;

      const isFontAdjust = !!e.shiftKey;
      const step = isFontAdjust ? 1 : 2;

      // Update UI immediately, persist with debounce to avoid flooding RPC.
      setAppSettings((prev) => {
        const next = { ...prev };
        if (isFontAdjust) {
          const cur = typeof prev.fontSize === 'number' ? prev.fontSize : 12;
          next.fontSize = Math.max(12, Math.min(20, cur - dir * step));
        } else {
          const cur = typeof prev.rowHeight === 'number' ? prev.rowHeight : 32;
          next.rowHeight = Math.max(18, Math.min(60, cur - dir * step));
        }
        wheelPendingSaveRef.current = { rowHeight: next.rowHeight ?? 32, fontSize: next.fontSize ?? 12 };
        return next;
      });

      if (wheelSaveTimerRef.current) window.clearTimeout(wheelSaveTimerRef.current);
      wheelSaveTimerRef.current = window.setTimeout(() => {
        const p = wheelPendingSaveRef.current ?? latestSettingsRef.current;
        wheelPendingSaveRef.current = null;
        void call('menu.saveSettings', { rowHeight: p.rowHeight, fontSize: p.fontSize } as any);
      }, 120);
    };
    window.addEventListener('wheel', onWheel, { passive: false, capture: true } as any);
    return () => window.removeEventListener('wheel', onWheel, { capture: true } as any);
  }, [call]);

  // WebUIBridge pushEvent listeners: openFolderResult / dropFolders / scanBusy
  useEffect(() => {
    const offOpenFolder = onEvent('openFolderResult', (data) => {
      if (!data || typeof data !== 'object') return;
      const path = (data as any).path;
      if (typeof path !== 'string' || !path) return;
      void (async () => {
        await call('sidebar.filterByFolder', { path });
        setFolderFilterPath(path);
        setStatus(`Folder: ${path}`);
      })();
    });

    const offScanBusy = onEvent('scanBusy', (data) => {
      const msg = (data && typeof data === 'object' && typeof (data as any).message === 'string') ? (data as any).message : 'scan is already running';
      setStatus(`Scan busy: ${msg}`);
      // keep it non-blocking; a real scan dialog will replace this later
    });

    const offDrop = onEvent('dropFolders', (data) => {
      if (!data || typeof data !== 'object') return;
      const folders = (data as any).folders;
      const summary = (data as any).summary;
      const first = Array.isArray(folders) && typeof folders[0] === 'string' ? folders[0] : '';
      setStatus(typeof summary === 'string' && summary ? summary : (first ? `Dropped: ${first}` : 'Dropped folders'));
      if (!first) return;

      // Minimal handling: ask user and start scan with current options.
      void (async () => {
        const opts = await call<any>('scan.getOptions');
        const ok = await confirmAsync(
          '开始扫描？',
          `检测到拖入文件夹：\n${first}\n\n${typeof summary === 'string' ? summary : ''}`,
        );
        if (!ok) return;
        await call('menu.scanFolder', {
          path: first,
          recursive: opts?.recursive,
          updateExisting: opts?.updateExisting,
          removeMissing: opts?.removeMissing,
          cacheWaveforms: opts?.cacheWaveforms,
          workerThreads: opts?.workerThreads,
        });
        setStatus(`Scanning: ${first}`);
      })();
    });

    return () => {
      offOpenFolder();
      offScanBusy();
      offDrop();
    };
  }, [call, confirmAsync]);

  // Debounced persist for spectrogram settings (match ImGui: stateful, not per-frame disk writes)
  useEffect(() => {
    if (spectrogramSaveTimerRef.current) {
      window.clearTimeout(spectrogramSaveTimerRef.current);
    }
    spectrogramSaveTimerRef.current = window.setTimeout(() => {
      void call('menu.saveSettings', {
        showSpectrogram,
        spectrogramSplit,
        spectrogramBlend,
        spectrogramFFTSize,
      });
    }, 250);
    return () => {
      if (spectrogramSaveTimerRef.current) window.clearTimeout(spectrogramSaveTimerRef.current);
    };
  }, [call, showSpectrogram, spectrogramBlend, spectrogramFFTSize, spectrogramSplit]);

  // Poll transport state (normalized position + duration + in/out)
  useEffect(() => {
    let alive = true;
    let inFlight = false;
    const tick = async () => {
      if (!alive || inFlight) return;
      inFlight = true;
      try {
        const s = await call<TransportState>('transport.getState');
        if (!alive || !s) return;
        setTransport((previous) => sameTransportState(previous, s) ? previous : s);
        setIsPlaying(!!s.playing);
        // Don't fight the user while dragging sliders (prevents jitter/jumping).
        if (!transportAdjustRef.current.pitch) setPitch(s.pitch ?? 0);
        if (!transportAdjustRef.current.volume) setVolume(s.volume ?? 1);
        const sec = (s.duration || 0) * (s.position || 0);
        setCurrentTime(sec);
      } finally {
        inFlight = false;
      }
    };
    void tick();
    const t = setInterval(() => void tick(), 120);
    return () => {
      alive = false;
      clearInterval(t);
    };
  }, [call]);

  // Global transport hotkey: Space toggles play/pause (except editable inputs).
  useEffect(() => {
    const isEditableTarget = (target: EventTarget | null): boolean => {
      if (!(target instanceof HTMLElement)) return false;
      const tag = target.tagName;
      if (tag === 'INPUT' || tag === 'TEXTAREA' || tag === 'SELECT') return true;
      if (target.isContentEditable) return true;
      return !!target.closest('input, textarea, select, [contenteditable="true"], [role="textbox"]');
    };

    const onKeyDown = (e: KeyboardEvent) => {
      if (e.code !== 'Space') return;
      if (e.repeat) return;
      if (e.ctrlKey || e.metaKey || e.altKey) return;
      if (isEditableTarget(e.target)) return;

      e.preventDefault();
      e.stopPropagation();

      if (transportPlayingRef.current) {
        void call('transport.pause');
        setIsPlaying(false);
        return;
      }

      const currentSelectedId = selectedIdRef.current;
      const currentPlayingId = transportFileIdRef.current;
      if (currentSelectedId && currentSelectedId > 0 && currentSelectedId !== currentPlayingId) {
        void call('fileList.play', { id: currentSelectedId });
      } else {
        void call('transport.play');
      }
      setIsPlaying(true);
    };

    window.addEventListener('keydown', onKeyDown, { capture: true });
    return () => window.removeEventListener('keydown', onKeyDown, { capture: true } as EventListenerOptions);
  }, [call]);

  // Player → selection link (ImGui: playerSelectionLink)
  useEffect(() => {
    const fileId = transport.fileId ?? null;
    if (!playerSelectionLink) return;
    if (!fileId || fileId <= 0) return;
    if (selectedId === fileId) return;

    setSelectedId(fileId);
    setSelectedIds(new Set([fileId]));
    lastClickedIndexRef.current = -1;
  }, [playerSelectionLink, selectedId, transport.fileId]);

  const effectiveQuery = appSettings.searchRealtime ? debouncedQuery : committedQuery;

  const reloadPage = useCallback(async () => {
    setLoading(true);
    const filters: SearchFilters = {
      showWAV: true,
      showMP3: true,
      showFLAC: true,
      showOGG: true,
      showAIFF: true,
      coverFilter,
      specificCoverKey: specificCoverKey || undefined,
    };
    const data = await call<SearchQueryResponse>('fileList.getPage', {
      query: effectiveQuery,
      limit: 500,
      offset: 0,
      sortColumn,
      sortAsc,
      filters,
    });
    if (data) {
      setRows(data.items ?? []);
      setTotal(data.total ?? 0);
    }
    setLoading(false);
  }, [call, coverFilter, effectiveQuery, sortAsc, sortColumn, specificCoverKey]);

  const syncSidebarFilters = useCallback(async (
    nextFilterState: FilterState = filterState,
    nextCoverFilter: 0 | 1 | 2 = coverFilter,
    nextSpecificCoverKey: string = specificCoverKey,
  ) => {
    await call('sidebar.applyFilters', {
      filters: {
        artist: nextFilterState.artist[0] ?? '',
        album: nextFilterState.album[0] ?? '',
        category: nextFilterState.category[0] ?? '',
        channels: nextFilterState.channels ?? 'any',
        sampleRate: nextFilterState.sampleRate[0] ?? 0,
        minDuration: nextFilterState.duration?.min ?? 0,
        maxDuration: nextFilterState.duration?.max ?? 0,
        coverFilter: nextCoverFilter,
        specificCoverKey: nextSpecificCoverKey || '',
      },
    });
  }, [call, coverFilter, filterState, specificCoverKey]);

  useEffect(() => {
    reloadPageRef.current = reloadPage;
  }, [reloadPage]);

  // Poll scan status so list becomes usable during scan (not only after completion).
  useEffect(() => {
    let alive = true;
    let inFlight = false;
    const tick = async () => {
      if (!alive || inFlight) return;
      inFlight = true;
      try {
        const st = await call<any>('scan.getStatus');
        if (!alive || !st || typeof st !== 'object') return;

        const scanning = !!st.scanning;
        const progress = Number(st.progress || 0);
        const totalFiles = Number(st.total || 0);
        const added = Number(st.added || 0);
        const updated = Number(st.updated || 0);
        const removed = Number(st.removed || 0);
        const currentFile = typeof st.currentFile === 'string' ? st.currentFile : '';
        const sig = `${progress}/${totalFiles}/${added}/${updated}/${removed}`;

        if (scanning) {
          if (totalFiles > 0) {
            setStatus(`正在发现... (${progress} / ${totalFiles})`);
          } else if (currentFile) {
            setStatus(currentFile);
          } else {
            setStatus('正在扫描...');
          }

          const now = Date.now();
          const changed = sig !== lastScanSigRef.current;
          const needFirstUsable = totalRef.current === 0 && (totalFiles > 0 || added > 0 || updated > 0);
          if ((needFirstUsable || changed) && (now - lastScanRefreshMsRef.current >= 800)) {
            lastScanRefreshMsRef.current = now;
            await reloadPageRef.current();
          }
          lastScanSigRef.current = sig;
        } else if (wasScanningRef.current) {
          setStatus('扫描完成');
          lastScanSigRef.current = '';
          const tree = await call<FolderNode>('sidebar.getFolderTree');
          if (tree) setFolderTree(tree);
          await reloadPageRef.current();
        }

        wasScanningRef.current = scanning;
      } finally {
        inFlight = false;
      }
    };

    void tick();
    const timer = window.setInterval(() => { void tick(); }, 700);
    return () => {
      alive = false;
      window.clearInterval(timer);
    };
  }, [call]);

  useEffect(() => {
    void reloadPage();
  }, [call, effectiveQuery, sortColumn, sortAsc, filterState, folderFilterPath, coverFilter, specificCoverKey]);

  const reloadDetails = useCallback(async () => {
    if (!selectedId) {
      setDetails(null);
      return;
    }
    const d = await call<FileInfoDetails>('fileInfo.getDetails', { id: selectedId });
    setDetails(d);
  }, [call, selectedId]);

  const reloadCover = useCallback(async () => {
    if (!selectedId) {
      setCover(null);
      return;
    }
    const c = await call<FileCoverResponse>('fileInfo.getCover', { id: selectedId });
    setCover(c);
  }, [call, selectedId]);

  const invalidateCoverThumbsForIds = useCallback((ids: number[]) => {
    if (!ids.length) return;
    for (const id of ids) {
      coverThumbByIdRef.current.delete(id);
      coverThumbByKeyRef.current.delete(`id:${id}`);
      coverInflightRef.current.delete(id);
      coverKeyByIdRef.current.delete(id);
    }
    scheduleCoverThumbRefresh();
  }, [scheduleCoverThumbRefresh]);

  const importArtworkForIds = useCallback(async (ids: number[], imagePath?: string) => {
    if (!ids.length) return false;
    const idsCsv = ids.join(',');
    const payload: Record<string, unknown> = { ids: idsCsv };
    if (imagePath) payload.imagePath = imagePath;
    const res = await call<any>('fileInfo.importCover', payload);
    if (!res) return false;
    if (res.ok === false && res.cancelled) {
      setStatus('导入封面已取消');
      return false;
    }
    const updated = Number(res.updated ?? 0);
    if (updated <= 0) {
      setStatus('未更新封面');
      return false;
    }
    invalidateCoverThumbsForIds(ids);
    await reloadDetails();
    await reloadPage();
    await reloadCover();
    setStatus(`已导入封面（${updated}）`);
    return true;
  }, [call, invalidateCoverThumbsForIds, reloadCover, reloadDetails, reloadPage]);

  const clearArtworkForIds = useCallback(async (ids: number[]) => {
    if (!ids.length) return false;
    const idsCsv = ids.join(',');
    const res = await call<any>('fileInfo.removeCover', { ids: idsCsv });
    if (!res) return false;
    if (res.ok === false && res.cancelled) {
      setStatus('清除封面已取消');
      return false;
    }
    const updated = Number(res.updated ?? 0);
    invalidateCoverThumbsForIds(ids);
    await reloadDetails();
    await reloadPage();
    await reloadCover();
    setStatus(updated > 0 ? `已清除封面（${updated}）` : '未清除封面');
    return updated > 0;
  }, [call, invalidateCoverThumbsForIds, reloadCover, reloadDetails, reloadPage]);

  useEffect(() => {
    const offCoverUpdated = onEvent('cover.updated', (data) => {
      const idsRaw = (data && typeof data === 'object') ? (data as any).ids : null;
      const ids = Array.isArray(idsRaw) ? idsRaw.map((v) => Number(v)).filter((v) => Number.isFinite(v) && v > 0) : [];
      if (ids.length) {
        invalidateCoverThumbsForIds(ids);
      }
      const folderPath = (data && typeof data === 'object' && typeof (data as any).folderPath === 'string') ? (data as any).folderPath : '';
      if (folderPath) {
        const matched = rows.filter((r) => r.folderPath === folderPath).map((r) => r.id);
        invalidateCoverThumbsForIds(matched);
      }
      void reloadPage();
      if (selectedId && ids.includes(selectedId)) void reloadCover();
    });
    return () => offCoverUpdated();
  }, [invalidateCoverThumbsForIds, reloadCover, reloadPage, rows, selectedId]);

  useEffect(() => {
    void reloadDetails();
    void reloadCover();
  }, [reloadCover, reloadDetails]);

  // Sync editor fields from details (single selection)
  useEffect(() => {
    if (!details) return;
    if (selectedIds.size !== 1) return;
    setEditDescription(details.description ?? '');
    setEditKeywords(details.keywords ?? '');
    setEditCategory(details.category ?? '');
  }, [details, selectedIds.size]);

  const displayRows = rows;
  const selectedRow = useMemo(() => displayRows.find((r) => r.id === selectedId) ?? null, [displayRows, selectedId]);
  const selectedIdsCsv = useMemo(() => Array.from(selectedIds).join(','), [selectedIds]);

  // Track visible row range (approx) for cover prefetch; avoid doing async work in render.
  useEffect(() => {
    const el = gridListRef.current;
    if (!el) return;
    let raf: number | null = null;
    const compute = () => {
      raf = null;
      const rowH = Math.max(16, Number(appSettings.rowHeight) || 32);
      const start = Math.max(0, Math.floor(el.scrollTop / rowH) - 10);
      const count = Math.ceil(el.clientHeight / rowH) + 30;
      const end = Math.min(displayRows.length, start + count);
      setResultVisibleRange((prev) => (prev.start === start && prev.end === end ? prev : { start, end }));
    };
    const onScroll = () => {
      if (raf != null) return;
      raf = window.requestAnimationFrame(compute);
    };
    compute();
    el.addEventListener('scroll', onScroll, { passive: true });
    window.addEventListener('resize', onScroll, { passive: true } as any);
    return () => {
      if (raf != null) window.cancelAnimationFrame(raf);
      el.removeEventListener('scroll', onScroll as any);
      window.removeEventListener('resize', onScroll as any);
    };
  }, [appSettings.rowHeight, displayRows.length]);

  // Prefetch cover thumbs for visible range (lazy + deduped). Keep it small to avoid UI hitching.
  useEffect(() => {
    const { start, end } = resultVisibleRange;
    if (end <= start) return;
    let cancelled = false;
    const keys: string[] = [];
    const keySet = new Set<string>();
    // 1) Fill by-id cache from key cache (no RPC) + collect missing keys for fetch.
    for (let i = start; i < end; i++) {
      const r = displayRows[i];
      if (!r?.hasCover) {
        coverThumbByIdRef.current.set(r.id, null);
        coverThumbByKeyRef.current.delete(`id:${r.id}`);
        coverKeyByIdRef.current.delete(r.id);
        continue;
      }
      const k = (r as any).coverKey as string | undefined;
      const rev = (r as any).coverRev as string | undefined;
      const compositeKey = `${k ?? ''}|${rev ?? ''}`;
      const lastKey = coverKeyByIdRef.current.get(r.id);
      if (lastKey !== compositeKey) {
        coverThumbByIdRef.current.delete(r.id);
        coverThumbByKeyRef.current.delete(`id:${r.id}`);
        coverKeyByIdRef.current.set(r.id, compositeKey);
      }
      if (k && coverThumbByKeyRef.current.has(k)) {
        if (!coverThumbByIdRef.current.has(r.id)) {
          coverThumbByIdRef.current.set(r.id, coverThumbByKeyRef.current.get(k)!);
        }
        continue;
      }
      if (k && !keySet.has(k) && keys.length < 24) {
        keySet.add(k);
        keys.push(k);
      }
      // Fallback: if no coverKey available, we can still fetch per id (rare).
      if (!k && !coverThumbByIdRef.current.has(r.id) && keys.length < 24) {
        // Use id-based method via existing ensureCoverThumb
        void ensureCoverThumb(r.id);
      }
    }

    if (keys.length) scheduleCoverThumbRefresh();

    // 2) Fetch missing thumbs by coverKey (distinct keys only).
    const run = async () => {
      for (const k of keys) {
        if (cancelled) break;
        if (coverThumbByKeyRef.current.has(k)) continue;
        if (coverKeyInflightRef.current.has(k)) continue;
        coverKeyInflightRef.current.add(k);
        try {
          const c = await call<FileCoverResponse>('cover.getThumb', { key: k });
          if (c?.dataUrl && typeof c.dataUrl === 'string' && c.dataUrl.startsWith('data:image/')) {
            coverThumbByKeyRef.current.set(k, c.dataUrl);
            // cap key cache
            const byKey = coverThumbByKeyRef.current;
            const kMaxKey = 128;
            if (byKey.size > kMaxKey) {
              const it = byKey.keys();
              while (byKey.size > kMaxKey) {
                const n = it.next();
                if (n.done) break;
                byKey.delete(n.value);
              }
            }
          }
        } finally {
          coverKeyInflightRef.current.delete(k);
          scheduleCoverThumbRefresh();
        }
      }
    };
    void run();
    return () => {
      cancelled = true;
    };
  }, [call, resultVisibleRange, displayRows, ensureCoverThumb, scheduleCoverThumbRefresh]);

  const onSort = (column: 'fileName' | 'duration' | 'sampleRate' | 'format') => {
    if (column === sortColumn) setSortAsc((v) => !v);
    else {
      setSortColumn(column);
      setSortAsc(true);
    }
  };

  const onFolderPick = async (path: string) => {
    await call('sidebar.filterByFolder', { path });
    setFolderFilterPath(path);
    setStatus(`Folder: ${path}`);
  };

  const shortenFolderPath = (path: string) => {
    if (!path) return '';
    const norm = path.replace(/\//g, '\\');
    const parts = norm.split('\\').filter(Boolean);
    if (parts.length <= 2) return norm;
    return `...\\${parts.slice(-2).join('\\')}`;
  };

  const activeChips = useMemo(() => {
    const chips: Array<{ key: string; label: string; rawValue: string; title: string }> = [];
    if (folderFilterPath) {
      chips.push({
        key: 'folder',
        label: `Folder: ${shortenFolderPath(folderFilterPath)}`,
        rawValue: folderFilterPath,
        title: folderFilterPath,
      });
    }
    if (coverFilter === 1) {
      chips.push({ key: 'cover', label: 'Cover: 有封面', rawValue: '1', title: '只显示有封面的文件' });
    } else if (coverFilter === 2) {
      chips.push({ key: 'cover', label: 'Cover: 无封面', rawValue: '2', title: '只显示无封面的文件' });
    }
    if (specificCoverKey) {
      chips.push({
        key: 'coverKey',
        label: 'CoverKey: 同封面',
        rawValue: specificCoverKey,
        title: specificCoverKey,
      });
    }
    // Path filters from query language: path:foo or path:"foo bar"
    const pathTokens: string[] = [];
    const re = /(?:^|\s)path:(\"[^\"]+\"|\S+)/gi;
    const q = effectiveQuery || '';
    let m: RegExpExecArray | null = null;
    while ((m = re.exec(q)) !== null) {
      let t = m[1] || '';
      t = t.trim();
      if (!t) continue;
      if (t.startsWith('"') && t.endsWith('"')) t = t.slice(1, -1);
      if (t) pathTokens.push(t);
    }
    pathTokens.slice(0, 6).forEach((t) => chips.push({ key: 'path', label: `Path: ${t}`, rawValue: t, title: `path:${t}` }));
    filterState.artist.forEach((v) => chips.push({ key: 'artist', label: `Artist: ${v}`, rawValue: v, title: v }));
    filterState.album.forEach((v) => chips.push({ key: 'album', label: `Album: ${v}`, rawValue: v, title: v }));
    filterState.category.forEach((v) => chips.push({ key: 'category', label: `Category: ${v}`, rawValue: v, title: v }));
    filterState.tags.forEach((v) => chips.push({ key: 'tags', label: `Tag: ${v}`, rawValue: v, title: v }));
    filterState.format.forEach((v) => chips.push({ key: 'format', label: `Format: ${v}`, rawValue: v, title: v }));
    filterState.sampleRate.forEach((v) => chips.push({ key: 'sampleRate', label: `Rate: ${v}`, rawValue: String(v), title: `${v}` }));
    if (filterState.channels && filterState.channels !== 'any') {
      chips.push({
        key: 'channels',
        label: `Channels: ${filterState.channels}`,
        rawValue: filterState.channels,
        title: filterState.channels,
      });
    }
    const min = filterState.duration?.min;
    const max = filterState.duration?.max;
    if (typeof min === 'number' || typeof max === 'number') {
      chips.push({
        key: 'duration',
        label: `Duration: ${typeof min === 'number' ? min : '-'}~${typeof max === 'number' ? max : '-'}`,
        rawValue: 'duration',
        title: 'Duration range',
      });
    }
    return chips;
  }, [coverFilter, effectiveQuery, filterState, folderFilterPath, specificCoverKey]);

  const clearAllFilters = async () => {
    setFilterState(defaultFilterState);
    setFolderFilterPath('');
    setCoverFilter(0);
    setSpecificCoverKey('');
    await call('sidebar.filterByFolder', { path: '' });
    await syncSidebarFilters(defaultFilterState, 0, '');
    setStatus('Filters cleared');
  };

  const removeActiveChip = async (key: string, rawValue: string) => {
    if (key === 'folder') {
      setFolderFilterPath('');
      await call('sidebar.filterByFolder', { path: '' });
      setStatus('Folder filter removed');
      return;
    }
    if (key === 'cover') {
      setCoverFilter(0);
      await syncSidebarFilters(filterState, 0, specificCoverKey);
      setStatus('Cover filter removed');
      return;
    }
    if (key === 'coverKey') {
      setSpecificCoverKey('');
      await syncSidebarFilters(filterState, coverFilter, '');
      setStatus('Cover key filter removed');
      return;
    }
    if (key === 'path') {
      // remove all occurrences of this path token
      const tok = rawValue.includes(' ') ? `path:"${rawValue}"` : `path:${rawValue}`;
      const next = (query || '').split(/\s+/).filter((t) => t && t.toLowerCase() !== tok.toLowerCase()).join(' ');
      setQuery(next);
      if (!appSettings.searchRealtime) setCommittedQuery(next);
      setStatus('Path filter removed');
      return;
    }
    const next: FilterState = { ...filterState };
    if (key === 'artist') next.artist = next.artist.filter((v) => v !== rawValue);
    if (key === 'album') next.album = next.album.filter((v) => v !== rawValue);
    if (key === 'category') next.category = next.category.filter((v) => v !== rawValue);
    if (key === 'tags') next.tags = next.tags.filter((v) => v !== rawValue);
    if (key === 'format') next.format = next.format.filter((v) => v !== rawValue);
    if (key === 'sampleRate') next.sampleRate = next.sampleRate.filter((v) => String(v) !== rawValue);
    if (key === 'channels') next.channels = 'any';
    if (key === 'duration') next.duration = {};
    setFilterState(next);
    await syncSidebarFilters(next, coverFilter, specificCoverKey);
    setStatus(`Filter removed: ${key}`);
  };

  const options: FilterOptions = useMemo(() => {
    const formatCount = new Map<string, number>();
    const categoryCount = new Map<string, number>();
    displayRows.forEach((r) => {
      const fmt = (r.format || r.extension || 'unknown').toUpperCase();
      formatCount.set(fmt, (formatCount.get(fmt) ?? 0) + 1);
      const cat = (r.category || 'Uncategorized');
      categoryCount.set(cat, (categoryCount.get(cat) ?? 0) + 1);
    });
    return {
      artist: [],
      album: [],
      category: [...categoryCount.entries()].map(([k, v]) => ({ value: k, label: k, count: v })).slice(0, 16),
      tags: [
        { value: 'SFX', label: 'SFX' },
        { value: 'Foley', label: 'Foley' },
        { value: 'Voice', label: 'Voice' },
        { value: 'Music', label: 'Music' },
      ],
      format: [...formatCount.entries()].map(([k, v]) => ({ value: k, label: k, count: v })),
      sampleRate: [
        { value: 44100, label: '44.1kHz' },
        { value: 48000, label: '48kHz' },
        { value: 96000, label: '96kHz' },
      ],
      channels: [
        { value: 'any', label: 'Any' },
        { value: 'mono', label: 'Mono' },
        { value: 'stereo', label: 'Stereo' },
      ],
    };
  }, [displayRows]);

  const selectedDuration = selectedRow?.durationSeconds ?? 0;
  const applyTransportAction = (action: 'play' | 'pause' | 'stop' | 'back' | 'forward') => {
    if (action === 'play' && selectedId) {
      void call('fileList.play', { id: selectedId });
      setIsPlaying(true);
      return;
    }
    if (action === 'pause') {
      void call('transport.pause');
      setIsPlaying(false);
      return;
    }
    if (action === 'stop') {
      void call('transport.stop');
      return;
    }
    if (action === 'back') {
      void call('playHistory.back');
      return;
    }
    if (action === 'forward') {
      void call('playHistory.forward');
      return;
    }
  };

  // ── MenuBar action dispatcher ──
  const handleMenuAction = useCallback(
    (method: string, params?: Record<string, unknown>) => {
      // Toggle-type settings
      if (method === 'menu.toggleSetting') {
        const key = params?.key as string;
        if (!key) return;
        setAppSettings((prev) => {
          const newVal = !(prev as any)[key];
          const updated = { ...prev, [key]: newVal };
          void call('menu.saveSettings', { [key]: newVal });
          // Sync individual states that are used elsewhere
          if (key === 'playerSelectionLink') setPlayerSelectionLink(newVal);
          if (key === 'showSpectrogram') setShowSpectrogram(newVal);
          if (key === 'useWebUIMain') setStatus(`主界面模式已切换为 ${newVal ? 'WebUI' : 'ImGui'}（重启生效）`);
          if (key === 'autoplayOnSelect' || key === 'continuousPlayback' || key === 'shufflePlayback') {
            // already saved via menu.saveSettings above
          }
          return updated;
        });
        return;
      }
      // Edit filename dialog
      if (method === 'menu.editFilename') {
        const id = params?.id as number;
        if (!id) return;
        const row = rows.find((r) => r.id === id);
        if (row) setEditFilenameDialog({ id: row.id, name: row.fileName });
        return;
      }
      // New DB dialog
      if (method === 'menu.newDb') {
        setNewDbDialog(true);
        setNewDbName('soundmanager');
        return;
      }
      // About dialog
      if (method === 'menu.about') {
        void (async () => {
          const info = await call<{ version: string; build: string; fileCount: number; dbPath: string }>('menu.about');
          if (info) setAboutInfo(info);
        })();
        return;
      }
      // Playlist selection
      if (method === 'menu.selectPlaylist') {
        // For now, just switch to the playlist view (future: dedicated playlist display)
        setStatus(`Switched to playlist ${params?.id}`);
        return;
      }
      // Create playlist — prompt name
      if (method === 'sidebar.createPlaylist') {
        void (async () => {
          const name = await promptAsync('新建播放列表', '播放列表名称');
          if (!name) return;
          await call('sidebar.createPlaylist', { name });
          const pls = await call<PlaylistItem[]>('sidebar.getPlaylists');
          if (pls) setPlaylists(pls);
        })();
        return;
      }
      // Refresh — after refresh, reload tree + results
      if (method === 'menu.refresh') {
        void (async () => {
          await call('menu.refresh');
          const tree = await call<FolderNode>('sidebar.getFolderTree');
          if (tree) setFolderTree(tree);
          // Trigger search re-fetch
          setStatus('Refreshed');
        })();
        return;
      }
      // Default: direct RPC call
      void (async () => {
        await call(method, params);
        // Refresh recent dbs after db operations
        if (method === 'menu.switchDb' || method === 'menu.openDb' || method === 'menu.clearRecentDbs') {
          const recent = await call<RecentDbInfo>('menu.getRecentDbs');
          if (recent) setRecentDbs(recent);
        }
      })();
    },
    [call, promptAsync, rows],
  );

  return (
    <div
      className="f-root"
      style={{
        ['--ui-font-size' as any]: `${appSettings.fontSize ?? 12}px`,
        ['--row-h' as any]: `${appSettings.rowHeight ?? 30}px`,
        ['--grid-cols' as any]: applyGridCols,
      }}
      onMouseDown={() => {
        // Click anywhere to dismiss context menu
        if (contextMenu) setContextMenu(null);
      }}
    >
      <div className="f-bg-holo" />
      <header className="f-top">
        <MenuBar
          onAction={handleMenuAction}
          recentDbs={recentDbs}
          playlists={playlists}
          settings={appSettings}
          selectedId={selectedId}
          fileCount={total}
        />
        <div className="f-toolbar-row">
          <div className="f-search-wrap">
            <input
              className="f-search-input"
              placeholder="搜索（多词用空格分隔）..."
              value={query}
              onChange={(e) => setQuery(e.target.value)}
              onKeyDown={(e) => {
                if (e.key === 'Enter' && !appSettings.searchRealtime) {
                  setCommittedQuery(query);
                }
              }}
            />
          </div>
          <div className="f-top-tools">
            <button
              className="f-icon-btn"
              title="Settings"
              onClick={() => {
                setSettingsOpen(true);
                void (async () => {
                  const settings = await call<AppSettingsDto>('menu.getSettings');
                  if (settings) setAppSettings((prev) => ({ ...prev, ...settings }));
                })();
              }}
            >
              ⚙
            </button>
            <button
              className="f-icon-btn"
              title={coverFilter === 0 ? '封面筛选：全部' : coverFilter === 1 ? '封面筛选：有封面' : '封面筛选：无封面'}
              onClick={() => {
                const nextCoverFilter = (coverFilter === 0 ? 1 : coverFilter === 1 ? 2 : 0) as 0 | 1 | 2;
                setSpecificCoverKey('');
                setCoverFilter(nextCoverFilter);
                void syncSidebarFilters(filterState, nextCoverFilter, '');
              }}
            >
              COV
            </button>
          </div>
          {!engineConnected && <div className="f-engine-status">ENGINE OFFLINE</div>}
        </div>
        <div className="f-active-filter-row">
          <div className="f-active-filter-scroller">
            {activeChips.map((chip) => (
              <span key={`${chip.key}:${chip.rawValue}`} className="f-active-filter-chip" title={chip.title}>
                <span className="f-active-filter-text">{chip.label}</span>
                <button
                  className="f-active-filter-close"
                  onClick={() => {
                    void removeActiveChip(chip.key, chip.rawValue);
                  }}
                >
                  ×
                </button>
              </span>
            ))}
          </div>
          {!!activeChips.length && (
            <button
              className="f-active-filter-clear"
              onClick={() => {
                void clearAllFilters();
              }}
            >
              Clear
            </button>
          )}
        </div>
      </header>

      <div className="f-main">
        <aside className="f-sidebar">
          <div className="f-tabline">
            <button className="f-tab f-tab-on">Library</button>
            <button className="f-tab">Data</button>
            <button className="f-tab">Tools</button>
          </div>
          <div className="f-side-section-title">FILE SYSTEM</div>
          <div className="f-tree-scroll">
            {folderTree ? (
              <FolderTree node={folderTree} onClick={onFolderPick} selectedPath={folderFilterPath} />
            ) : (
              <div className="f-dim">No folders</div>
            )}
          </div>
          <div className="f-side-section-title">Metadata Filters</div>
          <MetadataFilters
            value={filterState}
            options={options}
            defaultValue={defaultFilterState}
            onChange={(next) => {
              setFilterState(next);
              void syncSidebarFilters(next, coverFilter, specificCoverKey);
              setStatus('Filters updated');
            }}
            onReset={() => setStatus('Filters reset')}
            onClearAll={() => {
              void clearAllFilters();
            }}
            onRemoveChip={() => setStatus('Filter chip removed')}
            showChips={false}
          />
        </aside>

        <main className="f-center">
          <div
            className="f-grid-head"
            title="右键：显示/隐藏列"
            onContextMenu={(e) => {
              e.preventDefault();
              e.stopPropagation();
              if (contextMenu) setContextMenu(null);
              setColumnMenu({ x: e.clientX, y: e.clientY });
            }}
          >
            {visibleColOrder.map((k) => {
              const dndProps = {
                draggable: true,
                onDragStart: (e: React.DragEvent) => {
                  headerDragFromRef.current = k;
                  e.dataTransfer.setData('text/plain', k);
                  e.dataTransfer.effectAllowed = 'move';
                },
                onDragEnd: () => {
                  headerDragFromRef.current = null;
                },
                onDragOver: (e: React.DragEvent) => {
                  e.preventDefault();
                  e.dataTransfer.dropEffect = 'move';
                },
                onDrop: (e: React.DragEvent) => {
                  e.preventDefault();
                  const from = (e.dataTransfer.getData('text/plain') || headerDragFromRef.current || '') as ColKey;
                  if (!from || from === k) return;
                  const cur = colOrderRef.current.slice();
                  const a = cur.indexOf(from);
                  const b = cur.indexOf(k);
                  if (a < 0 || b < 0) return;
                  cur.splice(a, 1);
                  cur.splice(b, 0, from);
                  persistColumnOrder(cur);
                },
              };

              if (k === 'cover') {
                return (
                  <div key="cover" className="f-col-cover" {...dndProps}>
                    COVER
                    <span
                      className="f-col-resize"
                      draggable={false}
                      onPointerDown={(e) => {
                        (e.currentTarget as HTMLElement).setPointerCapture(e.pointerId);
                        gridDragRef.current = { key: 'cover', startX: e.clientX, startW: gridColsRef.current.cover, lastW: gridColsRef.current.cover };
                        e.preventDefault();
                        e.stopPropagation();
                      }}
                      onPointerMove={(e) => {
                        const st = gridDragRef.current;
                        if (!st || st.key !== 'cover') return;
                        if ((e.buttons & 1) === 0) return;
                        const dx = e.clientX - st.startX;
                        const w = Math.max(26, Math.min(64, Math.round(st.startW + dx)));
                        st.lastW = w;
                        gridColsRef.current = { ...gridColsRef.current, cover: w };
                        setGridCols((prev) => ({ ...prev, cover: w }));
                      }}
                      onPointerUp={() => {
                        const st = gridDragRef.current;
                        if (st?.key === 'cover' && typeof st.lastW === 'number') {
                          gridColsRef.current = { ...gridColsRef.current, cover: st.lastW };
                        }
                        if (st?.key === 'cover') persistGridCols({ ...gridColsRef.current });
                        gridDragRef.current = null;
                      }}
                      onPointerCancel={() => {
                        gridDragRef.current = null;
                      }}
                    />
                  </div>
                );
              }

              if (k === 'name') {
                return (
                  <button key="name" className="f-col-btn f-col-name" onClick={() => onSort('fileName')} {...dndProps}>
                    NAME
                  </button>
                );
              }

              if (k === 'wave') {
                return (
                  <div key="wave" className="f-col-wave" {...dndProps}>
                    WAVEFORM
                    <span
                      className="f-col-resize"
                      draggable={false}
                      onPointerDown={(e) => {
                        (e.currentTarget as HTMLElement).setPointerCapture(e.pointerId);
                        gridDragRef.current = { key: 'wave', startX: e.clientX, startW: gridColsRef.current.wave, lastW: gridColsRef.current.wave };
                        e.preventDefault();
                        e.stopPropagation();
                      }}
                      onPointerMove={(e) => {
                        const st = gridDragRef.current;
                        if (!st || st.key !== 'wave') return;
                        if ((e.buttons & 1) === 0) return;
                        const dx = e.clientX - st.startX;
                        const w = Math.max(90, Math.min(420, Math.round(st.startW + dx)));
                        st.lastW = w;
                        gridColsRef.current = { ...gridColsRef.current, wave: w };
                        setGridCols((prev) => ({ ...prev, wave: w }));
                      }}
                      onPointerUp={() => {
                        const st = gridDragRef.current;
                        if (st?.key === 'wave' && typeof st.lastW === 'number') {
                          gridColsRef.current = { ...gridColsRef.current, wave: st.lastW };
                        }
                        if (st?.key === 'wave') persistGridCols({ ...gridColsRef.current });
                        gridDragRef.current = null;
                      }}
                      onPointerCancel={() => {
                        gridDragRef.current = null;
                      }}
                    />
                  </div>
                );
              }

              if (k === 'duration') {
                return (
                  <button key="duration" className="f-col-btn" onClick={() => onSort('duration')} {...dndProps}>
                    DURATION
                    <span
                      className="f-col-resize"
                      draggable={false}
                      onPointerDown={(e) => {
                        (e.currentTarget as HTMLElement).setPointerCapture(e.pointerId);
                        gridDragRef.current = { key: 'duration', startX: e.clientX, startW: gridColsRef.current.duration, lastW: gridColsRef.current.duration };
                        e.preventDefault();
                        e.stopPropagation();
                      }}
                      onPointerMove={(e) => {
                        const st = gridDragRef.current;
                        if (!st || st.key !== 'duration') return;
                        if ((e.buttons & 1) === 0) return;
                        const dx = e.clientX - st.startX;
                        const w = Math.max(70, Math.min(220, Math.round(st.startW + dx)));
                        st.lastW = w;
                        gridColsRef.current = { ...gridColsRef.current, duration: w };
                        setGridCols((prev) => ({ ...prev, duration: w }));
                      }}
                      onPointerUp={() => {
                        const st = gridDragRef.current;
                        if (st?.key === 'duration' && typeof st.lastW === 'number') {
                          gridColsRef.current = { ...gridColsRef.current, duration: st.lastW };
                        }
                        if (st?.key === 'duration') persistGridCols({ ...gridColsRef.current });
                        gridDragRef.current = null;
                      }}
                      onPointerCancel={() => {
                        gridDragRef.current = null;
                      }}
                    />
                  </button>
                );
              }

              if (k === 'size') {
                return (
                  <div key="size" className="f-col-btn" {...dndProps}>
                    SIZE
                    <span
                      className="f-col-resize"
                      draggable={false}
                      onPointerDown={(e) => {
                        (e.currentTarget as HTMLElement).setPointerCapture(e.pointerId);
                        gridDragRef.current = { key: 'size', startX: e.clientX, startW: gridColsRef.current.size, lastW: gridColsRef.current.size };
                        e.preventDefault();
                        e.stopPropagation();
                      }}
                      onPointerMove={(e) => {
                        const st = gridDragRef.current;
                        if (!st || st.key !== 'size') return;
                        if ((e.buttons & 1) === 0) return;
                        const dx = e.clientX - st.startX;
                        const w = Math.max(70, Math.min(240, Math.round(st.startW + dx)));
                        st.lastW = w;
                        gridColsRef.current = { ...gridColsRef.current, size: w };
                        setGridCols((prev) => ({ ...prev, size: w }));
                      }}
                      onPointerUp={() => {
                        const st = gridDragRef.current;
                        if (st?.key === 'size' && typeof st.lastW === 'number') {
                          gridColsRef.current = { ...gridColsRef.current, size: st.lastW };
                        }
                        if (st?.key === 'size') persistGridCols({ ...gridColsRef.current });
                        gridDragRef.current = null;
                      }}
                      onPointerCancel={() => {
                        gridDragRef.current = null;
                      }}
                    />
                  </div>
                );
              }

              if (k === 'rate') {
                return (
                  <button key="rate" className="f-col-btn" onClick={() => onSort('sampleRate')} {...dndProps}>
                    RATE
                    <span
                      className="f-col-resize"
                      draggable={false}
                      onPointerDown={(e) => {
                        (e.currentTarget as HTMLElement).setPointerCapture(e.pointerId);
                        gridDragRef.current = { key: 'rate', startX: e.clientX, startW: gridColsRef.current.rate, lastW: gridColsRef.current.rate };
                        e.preventDefault();
                        e.stopPropagation();
                      }}
                      onPointerMove={(e) => {
                        const st = gridDragRef.current;
                        if (!st || st.key !== 'rate') return;
                        if ((e.buttons & 1) === 0) return;
                        const dx = e.clientX - st.startX;
                        const w = Math.max(70, Math.min(260, Math.round(st.startW + dx)));
                        st.lastW = w;
                        gridColsRef.current = { ...gridColsRef.current, rate: w };
                        setGridCols((prev) => ({ ...prev, rate: w }));
                      }}
                      onPointerUp={() => {
                        const st = gridDragRef.current;
                        if (st?.key === 'rate' && typeof st.lastW === 'number') {
                          gridColsRef.current = { ...gridColsRef.current, rate: st.lastW };
                        }
                        if (st?.key === 'rate') persistGridCols({ ...gridColsRef.current });
                        gridDragRef.current = null;
                      }}
                      onPointerCancel={() => {
                        gridDragRef.current = null;
                      }}
                    />
                  </button>
                );
              }

              if (k === 'fmt') {
                return (
                  <button key="fmt" className="f-col-btn" onClick={() => onSort('format')} {...dndProps}>
                    FMT
                    <span
                      className="f-col-resize"
                      draggable={false}
                      onPointerDown={(e) => {
                        (e.currentTarget as HTMLElement).setPointerCapture(e.pointerId);
                        gridDragRef.current = { key: 'fmt', startX: e.clientX, startW: gridColsRef.current.fmt, lastW: gridColsRef.current.fmt };
                        e.preventDefault();
                        e.stopPropagation();
                      }}
                      onPointerMove={(e) => {
                        const st = gridDragRef.current;
                        if (!st || st.key !== 'fmt') return;
                        if ((e.buttons & 1) === 0) return;
                        const dx = e.clientX - st.startX;
                        const w = Math.max(56, Math.min(200, Math.round(st.startW + dx)));
                        st.lastW = w;
                        gridColsRef.current = { ...gridColsRef.current, fmt: w };
                        setGridCols((prev) => ({ ...prev, fmt: w }));
                      }}
                      onPointerUp={() => {
                        const st = gridDragRef.current;
                        if (st?.key === 'fmt' && typeof st.lastW === 'number') {
                          gridColsRef.current = { ...gridColsRef.current, fmt: st.lastW };
                        }
                        if (st?.key === 'fmt') persistGridCols({ ...gridColsRef.current });
                        gridDragRef.current = null;
                      }}
                      onPointerCancel={() => {
                        gridDragRef.current = null;
                      }}
                    />
                  </button>
                );
              }

              return (
                <div key="category" className="f-col-btn" {...dndProps}>
                  CATEGORY
                  <span
                    className="f-col-resize"
                    draggable={false}
                    onPointerDown={(e) => {
                      (e.currentTarget as HTMLElement).setPointerCapture(e.pointerId);
                      gridDragRef.current = { key: 'category', startX: e.clientX, startW: gridColsRef.current.category, lastW: gridColsRef.current.category };
                      e.preventDefault();
                      e.stopPropagation();
                    }}
                    onPointerMove={(e) => {
                      const st = gridDragRef.current;
                      if (!st || st.key !== 'category') return;
                      if ((e.buttons & 1) === 0) return;
                      const dx = e.clientX - st.startX;
                      const w = Math.max(90, Math.min(420, Math.round(st.startW + dx)));
                      st.lastW = w;
                      gridColsRef.current = { ...gridColsRef.current, category: w };
                      setGridCols((prev) => ({ ...prev, category: w }));
                    }}
                    onPointerUp={() => {
                      const st = gridDragRef.current;
                      if (st?.key === 'category' && typeof st.lastW === 'number') {
                        gridColsRef.current = { ...gridColsRef.current, category: st.lastW };
                      }
                      if (st?.key === 'category') persistGridCols({ ...gridColsRef.current });
                      gridDragRef.current = null;
                    }}
                    onPointerCancel={() => {
                      gridDragRef.current = null;
                    }}
                  />
                </div>
              );
            })}
          </div>
          <div
            className="f-grid-list"
            ref={gridListRef}
            title="Ctrl+滚轮：调整行高｜Ctrl+Shift+滚轮：调整字体大小"
          >
            {displayRows.map((row, idx) => {
              const fmt = row.format || row.extension || '-';
              const isSel = selectedIds.has(row.id);
              return (
                <button
                  key={row.id}
                  id={`f-row-${row.id}`}
                  className={`f-row ${isSel ? 'f-row-on' : ''}`}
                  onPointerDown={(e) => {
                    if (e.button !== 0) return;
                    suppressRowClickUntilRef.current = 0;
                    listDragRef.current = { id: row.id, x: e.clientX, y: e.clientY, didDrag: false, pointerId: e.pointerId };
                    try {
                      (e.currentTarget as HTMLElement).setPointerCapture(e.pointerId);
                    } catch {
                      // ignore
                    }
                  }}
                  onPointerMove={(e) => {
                    const st = listDragRef.current;
                    if (!st || st.didDrag) return;
                    if (st.id !== row.id) return;
                    // left button still down?
                    if ((e.buttons & 1) === 0) return;
                    const dx = e.clientX - st.x;
                    const dy = e.clientY - st.y;
                    const dist = Math.hypot(dx, dy);
                    if (dist < 10) return;

                    st.didDrag = true;
                    // Suppress the following click triggered by pointer-up after an OLE drag.
                    suppressRowClickUntilRef.current = Date.now() + 500;

                    const ids = selectedIds.has(row.id) && selectedIds.size > 1 ? Array.from(selectedIds) : [row.id];
                    void call('fileList.startDrag', { ids }).finally(() => {
                      listDragRef.current = null;
                    });
                    e.preventDefault();
                    e.stopPropagation();
                  }}
                  onPointerUp={(e) => {
                    const st = listDragRef.current;
                    if (st?.pointerId === e.pointerId) listDragRef.current = null;
                  }}
                  onPointerCancel={(e) => {
                    const st = listDragRef.current;
                    if (st?.pointerId === e.pointerId) listDragRef.current = null;
                  }}
                  onClick={(e) => {
                    if (Date.now() < suppressRowClickUntilRef.current) {
                      e.preventDefault();
                      e.stopPropagation();
                      return;
                    }
                    const ctrl = e.ctrlKey || e.metaKey;
                    const shift = e.shiftKey;
                    if (shift && lastClickedIndexRef.current >= 0) {
                      const a = Math.min(lastClickedIndexRef.current, idx);
                      const b = Math.max(lastClickedIndexRef.current, idx);
                      const next = new Set<number>();
                      for (let i = a; i <= b; i++) next.add(displayRows[i].id);
                      setSelectedIds(next);
                    } else if (ctrl) {
                      const next = new Set(selectedIds);
                      if (next.has(row.id)) next.delete(row.id);
                      else next.add(row.id);
                      setSelectedIds(next);
                      lastClickedIndexRef.current = idx;
                    } else {
                      setSelectedIds(new Set([row.id]));
                      lastClickedIndexRef.current = idx;
                    }
                    setSelectedId(row.id);
                    // Sync selection to backend (ImGui waveform panel depends on selectedResultIndex)
                    void call('fileList.select', { id: row.id });
                  }}
                  onDoubleClick={() => void call('fileList.play', { id: row.id })}
                  onContextMenu={(e) => {
                    // WebView2 host disables default menus; show our own.
                    e.preventDefault();
                    e.stopPropagation();
                    setSelectedId(row.id);
                    setSelectedIds(new Set([row.id]));
                    lastClickedIndexRef.current = idx;
                    void call('fileList.select', { id: row.id });
                    setContextMenu({ x: e.clientX, y: e.clientY, row });
                  }}
                >
                  {visibleColOrder.map((k) => {
                    if (k === 'cover') {
                      const url = getCoverThumbDataUrl(row.id);
                      return (
                        <div key="cover" className="f-cell f-col-cover">
                          {row.hasCover && url ? (
                            <img className="f-cover-mini" src={url} alt="" draggable={false} />
                          ) : (
                            <span className="f-cover-mini ph" aria-label={row.hasCover ? 'cover loading' : 'no cover'} />
                          )}
                        </div>
                      );
                    }
                    if (k === 'name') {
                      return (
                        <div key="name" className="f-cell f-col-name">
                          <span className="f-col-name-text">
                            {appSettings.showFileExtension ? row.fileName : row.fileName.replace(/\.[^.\\\/]+$/, '')}
                          </span>
                        </div>
                      );
                    }
                    if (k === 'wave') {
                      return (
                        <div key="wave" className="f-cell f-col-wave">
                          <MiniWaveform
                            fileId={row.id}
                            width={Math.max(56, gridCols.wave - 16)}
                            height={18}
                            isPlaying={!!(transport.fileId && transport.fileId === row.id && transport.playing)}
                            playPos={transport.fileId === row.id ? (transport.position ?? 0) : 0}
                            renderActive={idx >= resultVisibleRange.start && idx < resultVisibleRange.end}
                          />
                        </div>
                      );
                    }
                    if (k === 'duration') return <div key="duration" className="f-cell">{formatDuration(row.durationSeconds ?? row.duration ?? 0)}</div>;
                    if (k === 'size') return <div key="size" className="f-cell">{((row.fileSize || 0) / (1024 * 1024)).toFixed(1)} MB</div>;
                    if (k === 'rate') return <div key="rate" className="f-cell">{row.sampleRate || '-'}</div>;
                    if (k === 'fmt') return <div key="fmt" className="f-cell"><span className="f-fmt-pill">{fmt.toUpperCase()}</span></div>;
                    return <div key="category" className="f-cell">{row.category || '-'}</div>;
                  })}
                </button>
              );
            })}
            {!displayRows.length && <div className="f-empty">{loading ? 'Loading...' : 'No data'}</div>}
          </div>
        </main>

        <aside className="f-inspector">
          <div className="f-inspector-title">
            <span>Inspector</span>
            <div className="f-inspector-actions">
              <button>☆</button><button>✎</button><button>↗</button><button>⋯</button>
            </div>
          </div>
          <div
            className={`f-cover${coverDropActive ? ' f-cover-drop-active' : ''}`}
            onClick={() => {
              if (cover?.dataUrl) setShowCoverPreview(true);
            }}
            onDragEnter={(e) => {
              e.preventDefault();
              if (!selectedIds.size) return;
              setCoverDropActive(true);
            }}
            onDragOver={(e) => {
              e.preventDefault();
              if (!selectedIds.size) return;
              e.dataTransfer.dropEffect = 'copy';
            }}
            onDragLeave={(e) => {
              e.preventDefault();
              if (!(e.currentTarget as HTMLElement).contains(e.relatedTarget as Node | null)) {
                setCoverDropActive(false);
              }
            }}
            onDrop={(e) => {
              e.preventDefault();
              setCoverDropActive(false);
              if (!selectedIds.size) return;
              const f = Array.from(e.dataTransfer.files || []).find((file) => /^image\/(jpeg|png|webp)$/i.test(file.type) || /\.(jpe?g|png|webp)$/i.test(file.name));
              if (!f) {
                setStatus('请拖入 JPG/PNG/WEBP 图片');
                return;
              }
              let p = (f as any).path as string | undefined;
              if (!p) {
                const uriList = e.dataTransfer.getData('text/uri-list');
                if (uriList) {
                  const first = uriList.split(/\r?\n/).find((x) => x && !x.startsWith('#'));
                  if (first && first.startsWith('file:///')) {
                    try {
                      p = decodeURIComponent(first.replace(/^file:\/\/\//i, '').replace(/\//g, '\\'));
                    } catch {}
                  }
                }
              }
              if (!p) {
                setStatus('当前环境未提供本地文件路径，改用“导入封面”');
                return;
              }
              void importArtworkForIds(Array.from(selectedIds), p);
            }}
            title={cover?.cacheKey || ''}
          >
            {cover?.dataUrl ? (
              <img className="f-cover-img" src={cover.dataUrl} alt="Cover" />
            ) : (
              <div className="f-cover-none">{cover?.source === 'too_large' ? '封面过大' : 'NO COVER'}</div>
            )}
          </div>
          <div className="f-cover-actions">
            <button
              className="f-cover-btn"
              disabled={selectedIds.size === 0}
              onClick={() => {
                if (!selectedIds.size) return;
                void importArtworkForIds(Array.from(selectedIds));
              }}
            >
              导入封面
            </button>
            <button
              className="f-cover-btn f-cover-btn-danger"
              disabled={selectedIds.size === 0}
              onClick={() => {
                if (!selectedIds.size) return;
                void (async () => {
                  const ok = await confirmAsync('删除封面', `确定要删除 ${selectedIds.size} 个文件的封面吗？此操作不可撤销。`);
                  if (!ok) return;
                  await clearArtworkForIds(Array.from(selectedIds));
                })();
              }}
            >
              删除封面
            </button>
            <button
              className="f-cover-btn"
              disabled={!cover?.dataUrl || !selectedRow}
              onClick={() => {
                if (!cover?.dataUrl || !selectedRow) return;
                // WebView2: trigger a normal browser download (no backend changes).
                const ext = cover.mime?.includes('png') ? 'png' : cover.mime?.includes('webp') ? 'webp' : 'jpg';
                const base = (selectedRow.fileName || 'cover').replace(/[\\\\/:*?\"<>|]+/g, '_').replace(/\\.[^.]+$/, '');
                const a = document.createElement('a');
                a.href = cover.dataUrl;
                a.download = `${base}.cover.${ext}`;
                a.rel = 'noopener';
                document.body.appendChild(a);
                a.click();
                a.remove();
                setStatus('Cover exported');
              }}
            >
              导出封面
            </button>
            <button
              className="f-cover-btn"
              disabled={!cover?.cacheKey}
              onClick={() => {
                if (!cover?.cacheKey) return;
                setCoverFilter(0);
                setSpecificCoverKey(cover.cacheKey);
                void syncSidebarFilters(filterState, 0, cover.cacheKey);
                setStatus('Cover key filter applied');
              }}
            >
              同封面筛选
            </button>
          </div>
          <div className="f-inspector-name">
            {(() => {
              const n = selectedRow?.fileName || '未选择文件';
              if (!selectedRow) return n;
              if (appSettings.showFileExtension) return n;
              return n.replace(/\.[^.\\\/]+$/, '');
            })()}
          </div>
          <div className="f-info-tabs">
            <span className="f-info-tab f-info-tab-on">Info</span>
            <span className="f-info-tab">Tags</span>
            <span className="f-info-tab">History</span>
          </div>
          <div className="f-info-grid">
            <div className="k">Format</div><div className="v">{details?.format || '-'}</div>
            <div className="k">Duration</div><div className="v">{details ? formatDuration(details.duration) : '-'}</div>
            <div className="k">Size</div><div className="v">{details ? `${Math.round(details.fileSize / 1024 / 10) / 100} MB` : '-'}</div>
            <div className="k">Created</div><div className="v">{details?.fileMtime || '-'}</div>
            <div className="k">SampleRate</div><div className="v">{details?.sampleRate || '-'}</div>
            <div className="k">Channels</div><div className="v">{details?.channels || '-'}</div>
          </div>
          <div className="f-meta-editor">
            {selectedIds.size === 0 && <div className="f-dim">未选择文件</div>}
            {selectedIds.size === 1 && (
              <>
                <div className="f-meta-row">
                  <div className="f-meta-label">Description</div>
                  <textarea className="f-meta-input" value={editDescription} onChange={(e) => setEditDescription(e.target.value)} />
                </div>
                <div className="f-meta-row">
                  <div className="f-meta-label">Keywords</div>
                  <textarea className="f-meta-input" value={editKeywords} onChange={(e) => setEditKeywords(e.target.value)} />
                </div>
                <div className="f-meta-row">
                  <div className="f-meta-label">Category</div>
                  <input className="f-meta-input-line" value={editCategory} onChange={(e) => setEditCategory(e.target.value)} />
                </div>
                <div className="f-meta-actions">
                  <button
                    className="f-meta-save"
                    disabled={!selectedId}
                    onClick={() => {
                      if (!selectedId) return;
                      void call('fileInfo.patchMetadata', {
                        id: selectedId,
                        description: editDescription,
                        keywords: editKeywords,
                        category: editCategory,
                      }).then(async () => {
                        await reloadDetails();
                        await reloadPage();
                        setStatus('Metadata saved');
                      });
                    }}
                  >
                    保存元数据
                  </button>
                </div>
              </>
            )}
            {selectedIds.size > 1 && (
              <>
                <div className="f-dim">已选择 {selectedIds.size} 项（批量编辑）</div>
                <div className="f-meta-row">
                  <label className="f-meta-check">
                    <input type="checkbox" checked={batchApplyDescription} onChange={(e) => setBatchApplyDescription(e.target.checked)} />
                    <span>Description</span>
                  </label>
                  <textarea className="f-meta-input" value={batchDescription} onChange={(e) => setBatchDescription(e.target.value)} disabled={!batchApplyDescription} />
                </div>
                <div className="f-meta-row">
                  <label className="f-meta-check">
                    <input type="checkbox" checked={batchApplyKeywords} onChange={(e) => setBatchApplyKeywords(e.target.checked)} />
                    <span>Keywords</span>
                  </label>
                  <textarea className="f-meta-input" value={batchKeywords} onChange={(e) => setBatchKeywords(e.target.value)} disabled={!batchApplyKeywords} />
                </div>
                <div className="f-meta-row">
                  <label className="f-meta-check">
                    <input type="checkbox" checked={batchApplyCategory} onChange={(e) => setBatchApplyCategory(e.target.checked)} />
                    <span>Category</span>
                  </label>
                  <input className="f-meta-input-line" value={batchCategory} onChange={(e) => setBatchCategory(e.target.value)} disabled={!batchApplyCategory} />
                </div>
                <div className="f-meta-actions">
                  <button
                    className="f-meta-save"
                    disabled={!selectedIdsCsv || (!batchApplyDescription && !batchApplyKeywords && !batchApplyCategory)}
                    onClick={() => {
                      const params: Record<string, unknown> = { ids: selectedIdsCsv };
                      if (batchApplyDescription) params.description = batchDescription;
                      if (batchApplyKeywords) params.keywords = batchKeywords;
                      if (batchApplyCategory) params.category = batchCategory;
                      void call('fileInfo.batchPatchMetadata', params).then(async () => {
                        await reloadDetails();
                        await reloadPage();
                        setStatus('Batch metadata saved');
                      });
                    }}
                  >
                    批量保存
                  </button>
                </div>
              </>
            )}
          </div>
        </aside>
      </div>

      <footer className="f-bottom">
        <div className="f-bottom-toolbar">
          <div className="f-timecode">{formatTimecode(currentTime, appSettings)}</div>
          <div className="f-transport-group">
            <button className="f-transport-btn" title="上一首" onClick={() => applyTransportAction('back')}>
              ⏮
            </button>
            <button className="f-transport-btn f-transport-primary" onClick={() => applyTransportAction(isPlaying ? 'pause' : 'play')}>
              {isPlaying ? '⏸' : '▶'}
            </button>
            <button className="f-transport-btn" title="下一首" onClick={() => applyTransportAction('forward')}>
              ⏭
            </button>
            <button className="f-transport-btn" title="停止" onClick={() => applyTransportAction('stop')}>
              ⏹
            </button>
            <button
              className="f-transport-btn f-transport-btn-rand"
              title="随机下一个（避开已播放，优先有波形缓存）"
              onClick={() => void call('transport.playRandom')}
            >
              🎲
            </button>
            <button
              className={`f-transport-btn f-transport-btn-rev${transport.reversed ? ' f-transport-btn-rev-on' : ''}`}
              title={transport.reversed ? '反转播放：ON（点击关闭）' : '反转播放：OFF（点击开启）'}
              onClick={() => {
                void call('transport.toggleReverse');
              }}
            >
              REV
            </button>
          </div>
          <div className="f-slider-pack">
            <label>Pitch</label>
            <input
              type="range"
              min={-24}
              max={24}
              step={0.1}
              value={pitch}
              onPointerDown={() => {
                transportAdjustRef.current.pitch = true;
              }}
              onPointerUp={() => {
                transportAdjustRef.current.pitch = false;
              }}
              onPointerCancel={() => {
                transportAdjustRef.current.pitch = false;
              }}
              onChange={(e) => {
                const v = parseFloat(e.target.value);
                setPitch(v);
                void call('transport.setPitch', { value: v });
              }}
            />
            <label>Volume</label>
            <input
              type="range"
              min={0}
              max={1.2}
              step={0.01}
              value={volume}
              onPointerDown={() => {
                transportAdjustRef.current.volume = true;
              }}
              onPointerUp={() => {
                transportAdjustRef.current.volume = false;
              }}
              onPointerCancel={() => {
                transportAdjustRef.current.volume = false;
              }}
              onChange={(e) => {
                const v = Math.max(0, Math.min(1.2, parseFloat(e.target.value)));
                setVolume(v);
                void call('transport.setVolume', { value: v });
              }}
            />
          </div>
          {showStatusInTransport && <div className="f-transport-status" title={status}>{status}</div>}
          <div className="f-transport-tail">
            <span className="f-transport-tail-id">{selectedRow?.id ?? 'IDLE'}</span>
            <span className="f-transport-tail-count">{rows.length ? total : displayRows.length} files</span>
          </div>
        </div>
      </footer>

      {contextMenu && (
        <div
          className="f-context-menu"
          style={{ left: `${contextMenu.x}px`, top: `${contextMenu.y}px` }}
          onMouseDown={(e) => e.stopPropagation()}
          onMouseLeave={() => setContextMenu(null)}
        >
          <button
            onClick={() => {
              void call('fileList.contextAction', { id: contextMenu.row.id, action: 'play' });
              setContextMenu(null);
            }}
          >
            播放
          </button>
          <button
            onClick={() => {
              void call('fileList.contextAction', { id: contextMenu.row.id, action: 'stop' });
              setContextMenu(null);
            }}
          >
            停止
          </button>
          <div className="f-context-sep" />
          <button
            onClick={() => {
              void call('fileList.contextAction', { id: contextMenu.row.id, action: 'revealInExplorer' });
              setContextMenu(null);
            }}
          >
            在资源管理器显示
          </button>
          <div className="f-context-sep" />
          <button
            onClick={() => {
              const ids = selectedIds.has(contextMenu.row.id) ? Array.from(selectedIds) : [contextMenu.row.id];
              void importArtworkForIds(ids);
              setContextMenu(null);
            }}
          >
            分配封面
          </button>
          <button
            onClick={() => {
              const ids = selectedIds.has(contextMenu.row.id) ? Array.from(selectedIds) : [contextMenu.row.id];
              void (async () => {
                const ok = await confirmAsync('清除封面', `确定清除 ${ids.length} 个文件的封面吗？`);
                if (!ok) return;
                await clearArtworkForIds(ids);
              })();
              setContextMenu(null);
            }}
          >
            清除封面
          </button>
          <button
            onClick={() => {
              void (async () => {
                const rs = await call<any>('fileInfo.setFolderCover', { id: contextMenu.row.id });
                if (rs?.ok) {
                  await reloadPage();
                  await reloadCover();
                  setStatus(`已设为文件夹封面（更新 ${Number(rs.updated ?? 0)}）`);
                }
              })();
              setContextMenu(null);
            }}
          >
            设为文件夹封面
          </button>
          <div className="f-context-sep" />
          <button
            onClick={() => {
              void (async () => {
                const c = await call<FileCoverResponse>('fileInfo.getCover', { id: contextMenu.row.id });
                if (!c?.dataUrl) {
                  setStatus('No cover to export');
                  setContextMenu(null);
                  return;
                }
                const ext = c.mime?.includes('png') ? 'png' : c.mime?.includes('webp') ? 'webp' : 'jpg';
                const base = (contextMenu.row.fileName || 'cover').replace(/[\\\\/:*?\"<>|]+/g, '_').replace(/\\.[^.]+$/, '');
                const a = document.createElement('a');
                a.href = c.dataUrl;
                a.download = `${base}.cover.${ext}`;
                a.rel = 'noopener';
                document.body.appendChild(a);
                a.click();
                a.remove();
                setStatus('Cover exported');
                setContextMenu(null);
              })();
            }}
          >
            导出封面
          </button>
          <button
            onClick={() => {
              void (async () => {
                const c = await call<FileCoverResponse>('fileInfo.getCover', { id: contextMenu.row.id });
                if (!c?.cacheKey) {
                  setStatus('No cover key');
                  setContextMenu(null);
                  return;
                }
                setCoverFilter(0);
                setSpecificCoverKey(c.cacheKey);
                await syncSidebarFilters(filterState, 0, c.cacheKey);
                setStatus('Cover key filter applied');
                setContextMenu(null);
              })();
            }}
          >
            同封面筛选
          </button>
          <button
            className="f-context-danger"
            onClick={() => {
              void call('fileList.contextAction', { id: contextMenu.row.id, action: 'removeFromDb' });
              setContextMenu(null);
            }}
          >
            从数据库移除
          </button>
        </div>
      )}

      {columnMenu && (
        <div
          className="f-context-menu f-col-ctx"
          style={{ left: `${columnMenu.x}px`, top: `${columnMenu.y}px` }}
          onMouseDown={(e) => e.stopPropagation()}
          onMouseLeave={() => setColumnMenu(null)}
        >
          <div className="f-context-title">列显示</div>
          {defaultColOrder.map((k) => {
            const visible = k === 'name' || !hiddenCols.has(k);
            const label =
              k === 'cover' ? 'Artwork' :
              k === 'name' ? 'Filename' :
              k === 'wave' ? 'Waveform' :
              k === 'duration' ? 'Duration' :
              k === 'size' ? 'Size' :
              k === 'rate' ? 'Rate' :
              k === 'fmt' ? 'Format' : 'Category';
            return (
              <button
                key={k}
                disabled={k === 'name'}
                onClick={() => {
                  toggleColumnHidden(k);
                }}
              >
                <span style={{ display: 'inline-block', width: 18 }}>{visible ? '✓' : ''}</span>
                {label}
              </button>
            );
          })}
          <div className="f-context-sep" />
          <button
            onClick={() => {
              resetColumnsToDefault();
              setColumnMenu(null);
            }}
          >
            重置列显示（默认布局）
          </button>
          <button
            onClick={() => {
              autofitColumnsToContent();
              setColumnMenu(null);
            }}
          >
            自适应列宽（当前内容）
          </button>
        </div>
      )}

      {/* ── Cover Preview ── */}
      {showCoverPreview && cover?.dataUrl && (
        <div className="modal-overlay" onClick={() => setShowCoverPreview(false)}>
          <div className="modal-dialog f-cover-preview-modal" onClick={(e) => e.stopPropagation()}>
            <img className="f-cover-preview-img" src={cover.dataUrl} alt="Cover Preview" />
            <div className="modal-actions">
              <button className="btn-primary" onClick={() => setShowCoverPreview(false)}>关闭</button>
            </div>
          </div>
        </div>
      )}

      {/* ── About Dialog ── */}
      {aboutInfo && (
        <div className="modal-overlay" onClick={() => setAboutInfo(null)}>
          <div className="modal-dialog" onClick={(e) => e.stopPropagation()}>
            <h3 style={{ margin: '0 0 12px', fontSize: 'var(--fs-16)', fontWeight: 600, color: 'rgba(180,220,255,0.95)' }}>关于 Harken Search</h3>
            <div style={{ fontSize: 'var(--fs-13)', lineHeight: 1.8, color: 'rgba(200,220,240,0.85)' }}>
              <div>版本: {aboutInfo.version} ({aboutInfo.build})</div>
              <div>文件总数: {aboutInfo.fileCount}</div>
              <div style={{ wordBreak: 'break-all' }}>数据库: {aboutInfo.dbPath}</div>
            </div>
            <div style={{ marginTop: 16, textAlign: 'right' }}>
              <button className="f-menu-btn" onClick={() => setAboutInfo(null)}>关闭</button>
            </div>
          </div>
        </div>
      )}

      {/* ── Edit Filename Dialog ── */}
      {editFilenameDialog && (
        <div className="modal-overlay" onClick={() => setEditFilenameDialog(null)}>
          <div className="modal-dialog" onClick={(e) => e.stopPropagation()}>
            <h3 style={{ margin: '0 0 12px', fontSize: 'var(--fs-16)', fontWeight: 600, color: 'rgba(180,220,255,0.95)' }}>编辑文件名</h3>
            <input
              className="f-search-input"
              style={{ width: '100%', marginBottom: 12 }}
              value={editFilenameDialog.name}
              onChange={(e) => setEditFilenameDialog({ ...editFilenameDialog, name: e.target.value })}
              onKeyDown={(e) => {
                if (e.key === 'Enter') {
                  void (async () => {
                    await call('menu.editFilename', { id: editFilenameDialog.id, newName: editFilenameDialog.name });
                    setEditFilenameDialog(null);
                    setStatus('文件已重命名');
                  })();
                }
              }}
              autoFocus
            />
            <div style={{ display: 'flex', justifyContent: 'flex-end', gap: 8 }}>
              <button className="f-menu-btn" onClick={() => setEditFilenameDialog(null)}>取消</button>
              <button
                className="f-menu-btn"
                onClick={() => {
                  void (async () => {
                    await call('menu.editFilename', { id: editFilenameDialog.id, newName: editFilenameDialog.name });
                    setEditFilenameDialog(null);
                    setStatus('文件已重命名');
                  })();
                }}
              >
                确定
              </button>
            </div>
          </div>
        </div>
      )}

      {/* ── New Database Dialog ── */}
      {newDbDialog && (
        <div className="modal-overlay" onClick={() => setNewDbDialog(false)}>
          <div className="modal-dialog" onClick={(e) => e.stopPropagation()}>
            <h3 style={{ margin: '0 0 12px', fontSize: 'var(--fs-16)', fontWeight: 600, color: 'rgba(180,220,255,0.95)' }}>新建数据库</h3>
            <div style={{ fontSize: 'var(--fs-12)', color: 'rgba(160,190,230,0.7)', marginBottom: 8 }}>数据库名称：</div>
            <input
              className="f-search-input"
              style={{ width: '100%', marginBottom: 12 }}
              value={newDbName}
              onChange={(e) => setNewDbName(e.target.value)}
              onKeyDown={(e) => {
                if (e.key === 'Enter') {
                  void (async () => {
                    await call('menu.newDb', { name: newDbName });
                    setNewDbDialog(false);
                    const recent = await call<RecentDbInfo>('menu.getRecentDbs');
                    if (recent) setRecentDbs(recent);
                    const tree = await call<FolderNode>('sidebar.getFolderTree');
                    if (tree) setFolderTree(tree);
                    setStatus(`新数据库已创建: ${newDbName}`);
                  })();
                }
              }}
              autoFocus
            />
            <div style={{ display: 'flex', justifyContent: 'flex-end', gap: 8 }}>
              <button className="f-menu-btn" onClick={() => setNewDbDialog(false)}>取消</button>
              <button
                className="f-menu-btn"
                onClick={() => {
                  void (async () => {
                    await call('menu.newDb', { name: newDbName });
                    setNewDbDialog(false);
                    const recent = await call<RecentDbInfo>('menu.getRecentDbs');
                    if (recent) setRecentDbs(recent);
                    const tree = await call<FolderNode>('sidebar.getFolderTree');
                    if (tree) setFolderTree(tree);
                    setStatus(`新数据库已创建: ${newDbName}`);
                  })();
                }}
              >
                创建
              </button>
            </div>
          </div>
        </div>
      )}

      {/* ── Confirm Dialog (replace window.confirm) ── */}
      {confirmDialog && (
        <div
          className="modal-overlay"
          onClick={() => {
            confirmDialog.resolve(false);
            setConfirmDialog(null);
          }}
        >
          <div className="modal-dialog" onClick={(e) => e.stopPropagation()}>
            <h3 style={{ margin: '0 0 12px', fontSize: 'var(--fs-16)', fontWeight: 600, color: 'rgba(180,220,255,0.95)' }}>{confirmDialog.title}</h3>
            <div style={{ fontSize: 'var(--fs-13)', lineHeight: 1.8, color: 'rgba(200,220,240,0.85)', whiteSpace: 'pre-wrap' }}>
              {confirmDialog.message}
            </div>
            <div className="modal-actions">
              <button
                className="f-menu-btn"
                onClick={() => {
                  confirmDialog.resolve(false);
                  setConfirmDialog(null);
                }}
              >
                {confirmDialog.cancelText ?? '取消'}
              </button>
              <button
                className="f-menu-btn"
                onClick={() => {
                  confirmDialog.resolve(true);
                  setConfirmDialog(null);
                }}
              >
                {confirmDialog.okText ?? '确定'}
              </button>
            </div>
          </div>
        </div>
      )}

      {/* ── Prompt Dialog (replace window.prompt) ── */}
      {promptDialog && (
        <div
          className="modal-overlay"
          onClick={() => {
            promptDialog.resolve(null);
            setPromptDialog(null);
          }}
        >
          <div className="modal-dialog" onClick={(e) => e.stopPropagation()}>
            <h3 style={{ margin: '0 0 12px', fontSize: 'var(--fs-16)', fontWeight: 600, color: 'rgba(180,220,255,0.95)' }}>{promptDialog.title}</h3>
            <div style={{ fontSize: 'var(--fs-12)', color: 'rgba(160,190,230,0.7)', marginBottom: 8 }}>{promptDialog.label}</div>
            <input
              className="f-search-input"
              style={{ width: '100%', marginBottom: 12 }}
              value={promptText}
              placeholder={promptDialog.placeholder ?? ''}
              onChange={(e) => setPromptText(e.target.value)}
              onKeyDown={(e) => {
                if (e.key === 'Enter') {
                  const v = promptText.trim();
                  promptDialog.resolve(v ? v : null);
                  setPromptDialog(null);
                }
              }}
              autoFocus
            />
            <div className="modal-actions">
              <button
                className="f-menu-btn"
                onClick={() => {
                  promptDialog.resolve(null);
                  setPromptDialog(null);
                }}
              >
                {promptDialog.cancelText ?? '取消'}
              </button>
              <button
                className="f-menu-btn"
                onClick={() => {
                  const v = promptText.trim();
                  promptDialog.resolve(v ? v : null);
                  setPromptDialog(null);
                }}
              >
                {promptDialog.okText ?? '确定'}
              </button>
            </div>
          </div>
        </div>
      )}

      {/* ── Settings Dialog ── */}
      {settingsOpen && (
        <div className="modal-overlay" onClick={() => setSettingsOpen(false)}>
          <div className="modal-dialog settings-modal" onClick={(e) => e.stopPropagation()}>
            <div className="settings-head">
              <div className="settings-title">Settings</div>
              <button className="settings-close" onClick={() => setSettingsOpen(false)}>×</button>
            </div>

            <div className="settings-body">
              <div className="settings-section">
                <div className="settings-section-title">Theme</div>
                <div className="settings-theme-row">
                  <button className="theme-pill theme-pill-on">Midnight Blue</button>
                  <button className="theme-pill" disabled>Polar White</button>
                  <button className="theme-pill" disabled>Cyberpunk Neon</button>
                </div>
              </div>

              <div className="settings-section">
                <div className="settings-section-title">Appearance</div>
                <div className="settings-grid">
                  <div className="settings-field">
                    <div className="settings-label">Font Size</div>
                    <select
                      className="settings-select"
                      value={
                        (appSettings.fontSize ?? 12) <= 12 ? '12' : (appSettings.fontSize ?? 12) <= 14 ? '14' : '16'
                      }
                      onChange={(e) => {
                        const v = parseInt(e.target.value, 10);
                        saveSettingsPatch({ fontSize: v });
                        setStatus('Settings saved');
                      }}
                    >
                      <option value="12">Small</option>
                      <option value="14">Medium</option>
                      <option value="16">Large</option>
                    </select>
                  </div>
                  <div className="settings-field">
                    <div className="settings-label">Row Height</div>
                    <select
                      className="settings-select"
                      value={
                        (appSettings.rowHeight ?? 32) <= 22
                          ? '20'
                          : (appSettings.rowHeight ?? 32) <= 26
                            ? '24'
                            : (appSettings.rowHeight ?? 32) <= 30
                              ? '28'
                              : (appSettings.rowHeight ?? 32) <= 35
                                ? '32'
                                : '40'
                      }
                      onChange={(e) => {
                        const v = parseInt(e.target.value, 10);
                        saveSettingsPatch({ rowHeight: v });
                        setStatus('Settings saved');
                      }}
                    >
                      <option value="20">Ultra Compact</option>
                      <option value="24">Very Compact</option>
                      <option value="28">Compact</option>
                      <option value="32">Default</option>
                      <option value="40">Comfort</option>
                    </select>
                  </div>
                </div>
              </div>

              <div className="settings-section">
                <div className="settings-section-title">Behavior</div>

                <div className="settings-row">
                  <div className="settings-label">Playback Mode</div>
                  <div className="settings-seg">
                    <button
                      className={`settings-seg-btn${appSettings.playbackMode === 0 ? ' on' : ''}`}
                      onClick={() => saveSettingsPatch({ playbackMode: 0 })}
                    >
                      单击
                    </button>
                    <button
                      className={`settings-seg-btn${appSettings.playbackMode === 1 ? ' on' : ''}`}
                      onClick={() => saveSettingsPatch({ playbackMode: 1 })}
                    >
                      双击
                    </button>
                    <button
                      className={`settings-seg-btn${appSettings.playbackMode === 2 ? ' on' : ''}`}
                      onClick={() => saveSettingsPatch({ playbackMode: 2 })}
                    >
                      切换
                    </button>
                  </div>
                </div>

                <div className="settings-row">
                  <div className="settings-label">Auto-play on selection</div>
                  <label className="settings-switch">
                    <input
                      type="checkbox"
                      checked={!!appSettings.autoplayOnSelect}
                      onChange={(e) => saveSettingsPatch({ autoplayOnSelect: e.target.checked })}
                    />
                    <span />
                  </label>
                </div>
                <div className="settings-row">
                  <div className="settings-label">Continuous playback</div>
                  <label className="settings-switch">
                    <input
                      type="checkbox"
                      checked={!!appSettings.continuousPlayback}
                      onChange={(e) => saveSettingsPatch({ continuousPlayback: e.target.checked })}
                    />
                    <span />
                  </label>
                </div>
                <div className="settings-row">
                  <div className="settings-label">Shuffle playback</div>
                  <label className="settings-switch">
                    <input
                      type="checkbox"
                      checked={!!appSettings.shufflePlayback}
                      onChange={(e) => saveSettingsPatch({ shufflePlayback: e.target.checked })}
                    />
                    <span />
                  </label>
                </div>
                <div className="settings-row">
                  <div className="settings-label">Search realtime</div>
                  <label className="settings-switch">
                    <input
                      type="checkbox"
                      checked={!!appSettings.searchRealtime}
                      onChange={(e) => {
                        saveSettingsPatch({ searchRealtime: e.target.checked });
                        if (e.target.checked) setCommittedQuery(query);
                      }}
                    />
                    <span />
                  </label>
                </div>
                <div className="settings-row">
                  <div className="settings-label">Show file extension</div>
                  <label className="settings-switch">
                    <input
                      type="checkbox"
                      checked={!!appSettings.showFileExtension}
                      onChange={(e) => saveSettingsPatch({ showFileExtension: e.target.checked })}
                    />
                    <span />
                  </label>
                </div>
                <div className="settings-row">
                  <div className="settings-label">Use WebUI main screen (restart required)</div>
                  <label className="settings-switch">
                    <input
                      type="checkbox"
                      checked={!!appSettings.useWebUIMain}
                      onChange={(e) => {
                        saveSettingsPatch({ useWebUIMain: e.target.checked });
                        setStatus(`主界面模式已切换为 ${e.target.checked ? 'WebUI' : 'ImGui'}（重启生效）`);
                      }}
                    />
                    <span />
                  </label>
                </div>
                <div className="settings-row">
                  <div className="settings-label">Show milliseconds</div>
                  <label className="settings-switch">
                    <input
                      type="checkbox"
                      checked={appSettings.showMilliseconds !== false}
                      onChange={(e) => saveSettingsPatch({ showMilliseconds: e.target.checked })}
                    />
                    <span />
                  </label>
                </div>
                <div className="settings-row">
                  <div className="settings-label">Show frame number</div>
                  <label className="settings-switch">
                    <input
                      type="checkbox"
                      checked={!!appSettings.showFrameNumber}
                      onChange={(e) => saveSettingsPatch({ showFrameNumber: e.target.checked })}
                    />
                    <span />
                  </label>
                </div>
                {appSettings.showFrameNumber && (
                  <div className="settings-row">
                    <div className="settings-label">Default frame rate</div>
                    <input
                      className="settings-input"
                      type="number"
                      min={1}
                      max={120}
                      step={0.25}
                      value={appSettings.defaultFrameRate ?? 25}
                      onChange={(e) => saveSettingsPatch({ defaultFrameRate: parseFloat(e.target.value) })}
                    />
                  </div>
                )}
              </div>

              <div className="settings-section">
                <div className="settings-section-title">Spectrogram</div>
                <div className="settings-row">
                  <div className="settings-label">Show spectrogram</div>
                  <label className="settings-switch">
                    <input
                      type="checkbox"
                      checked={!!showSpectrogram}
                      onChange={(e) => {
                        setShowSpectrogram(e.target.checked);
                        setAppSettings((prev) => ({ ...prev, showSpectrogram: e.target.checked }));
                      }}
                    />
                    <span />
                  </label>
                </div>
                <div className="settings-row">
                  <div className="settings-label">Split</div>
                  <label className="settings-switch">
                    <input
                      type="checkbox"
                      checked={!!spectrogramSplit}
                      onChange={(e) => {
                        setSpectrogramSplit(e.target.checked);
                        setAppSettings((prev) => ({ ...prev, spectrogramSplit: e.target.checked }));
                      }}
                    />
                    <span />
                  </label>
                </div>
                <div className="settings-row">
                  <div className="settings-label">Blend</div>
                  <input
                    type="range"
                    min={0}
                    max={1}
                    step={0.01}
                    value={spectrogramBlend}
                    onChange={(e) => {
                      const v = parseFloat(e.target.value);
                      setSpectrogramBlend(v);
                      setAppSettings((prev) => ({ ...prev, spectrogramBlend: v }));
                    }}
                  />
                </div>
                <div className="settings-row">
                  <div className="settings-label">FFT</div>
                  <select
                    className="settings-select"
                    value={String(spectrogramFFTSize)}
                    onChange={(e) => {
                      const v = parseInt(e.target.value, 10);
                      setSpectrogramFFTSize(v);
                      setAppSettings((prev) => ({ ...prev, spectrogramFFTSize: v }));
                    }}
                  >
                    <option value="1024">1024</option>
                    <option value="2048">2048</option>
                    <option value="4096">4096</option>
                    <option value="8192">8192</option>
                    <option value="16384">16384</option>
                  </select>
                </div>
              </div>

              <div className="settings-section">
                <div className="settings-section-title">Audio Engine</div>
                <div className="settings-row">
                  <div className="settings-label">Status</div>
                  <span className={`settings-pill${engineConnected ? ' on' : ''}`}>{engineConnected ? 'active' : 'offline'}</span>
                </div>
              </div>
            </div>

            <div className="settings-foot">
              <button className="btn-primary" onClick={() => setSettingsOpen(false)}>Done</button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}

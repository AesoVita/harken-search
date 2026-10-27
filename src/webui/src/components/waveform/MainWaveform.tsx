import React, { useEffect, useMemo, useRef, useState } from 'react';
import { invoke } from '../../bridge';
import type { SpectrogramTileResponse } from '../../types';

export type MainWaveformData = {
  peaks: number[];
  rms?: number[];
};

export type MainWaveformTransport = {
  playing: boolean;
  loaded: boolean;
  position: number; // normalized 0..1
  duration: number; // seconds
  volume?: number; // 0..1.5 (visual scale)
  inPoint: number; // normalized 0..1
  outPoint: number; // normalized 0..1
};

function clamp(v: number, a: number, b: number) {
  return Math.min(b, Math.max(a, v));
}

function fmtTime(sec: number) {
  if (!isFinite(sec) || sec < 0) sec = 0;
  const m = Math.floor(sec / 60);
  const s = Math.floor(sec % 60);
  const ms = Math.floor((sec - Math.floor(sec)) * 1000);
  return `${m}:${String(s).padStart(2, '0')}.${String(ms).padStart(3, '0')}`;
}

function niceStep(targetSec: number) {
  // Pick from a small "music friendly" set.
  const steps = [0.1, 0.2, 0.5, 1, 2, 5, 10, 15, 30, 60, 120, 300, 600];
  for (const s of steps) if (s >= targetSec) return s;
  return steps[steps.length - 1];
}

type BaseLayerCache = {
  key: string;
  canvas: HTMLCanvasElement;
};

export default function MainWaveform(props: {
  className?: string;
  data: MainWaveformData | null;
  transport: MainWaveformTransport;
  fileId?: number | null;
  spectrogram?: {
    enabled: boolean;
    split: boolean;
    blend: number; // 0..1
    fftSize: number;
  };
  onSpectrogramBlendChange?: (nextBlend: number) => void;
}) {
  const { className, data, transport, fileId, spectrogram, onSpectrogramBlendChange } = props;
  const wrapRef = useRef<HTMLDivElement | null>(null);
  const canvasRef = useRef<HTMLCanvasElement | null>(null);
  const dpr = typeof window !== 'undefined' ? (window.devicePixelRatio || 1) : 1;
  const specOffscreenRef = useRef<HTMLCanvasElement | null>(null);
  const specKeyRef = useRef<string>('');
  const specReqSeqRef = useRef(1);
  const [wrapSize, setWrapSize] = useState<{ w: number; h: number }>({ w: 0, h: 0 });
  const baseLayerRef = useRef<BaseLayerCache | null>(null);

  // Smooth playhead: keep last backend sample and locally integrate by time (like ImGui's per-frame rendering).
  const transportSampleRef = useRef<{
    tMs: number;
    position: number;
    duration: number;
    playing: boolean;
  }>({ tMs: performance.now(), position: 0, duration: 0, playing: false });
  const smoothPlayheadRef = useRef<number>(0);

  // View state (WebUI mirrors ImGui: zoom + viewCenter + click anchor)
  const [zoom, setZoom] = useState(1.0);
  const [viewCenter, setViewCenter] = useState(0.5);
  const anchorRef = useRef<{ set: boolean; value: number }>({ set: false, value: 0 });

  // Drag state
  const dragRef = useRef<{
    mode: 'none' | 'region' | 'export';
    startNorm: number;
    startX: number;
    startY: number;
    pendingSeek: boolean;
    rafPending: boolean;
    latestA: number;
    latestB: number;
  }>({
    mode: 'none',
    startNorm: 0,
    startX: 0,
    startY: 0,
    pendingSeek: false,
    rafPending: false,
    latestA: 0,
    latestB: 0,
  });

  const view = useMemo(() => {
    const z = clamp(zoom, 1.0, 20.0);
    const win = 1.0 / z;
    const start = z > 1.0 ? clamp(viewCenter - win * 0.5, 0.0, 1.0 - win) : 0.0;
    const end = start + win;
    return { z, win, start, end };
  }, [zoom, viewCenter]);

  const mouseXToNorm = (clientX: number) => {
    const wrap = wrapRef.current;
    if (!wrap) return 0;
    const r = wrap.getBoundingClientRect();
    const t = clamp((clientX - r.left) / r.width, 0, 1);
    return view.start + t * view.win;
  };

  const hasRegion = transport.inPoint > 0.0 || transport.outPoint < 1.0;

  // Update transport sample for playhead interpolation.
  useEffect(() => {
    transportSampleRef.current = {
      tMs: performance.now(),
      position: clamp(transport.position ?? 0, 0, 1),
      duration: Math.max(0, transport.duration ?? 0),
      playing: !!transport.playing,
    };
    if (!transport.playing) {
      smoothPlayheadRef.current = clamp(transport.position ?? 0, 0, 1);
    }
  }, [transport.duration, transport.playing, transport.position]);

  // Track size changes (for correct tile requests)
  useEffect(() => {
    const el = wrapRef.current;
    if (!el) return;
    const ro = new ResizeObserver(() => {
      setWrapSize({ w: el.clientWidth, h: el.clientHeight });
    });
    ro.observe(el);
    setWrapSize({ w: el.clientWidth, h: el.clientHeight });
    return () => ro.disconnect();
  }, []);

  // Fetch spectrogram tile matching ImGui UV behavior (we request already-cropped window [view.start..view.end])
  useEffect(() => {
    if (!spectrogram?.enabled) {
      specOffscreenRef.current = null;
      specKeyRef.current = '';
      return;
    }
    if (!fileId || fileId <= 0) return;
    if (!transport.loaded) return;
    const el = wrapRef.current;
    if (!el) return;

    // ImGui draws spectrogram on waveform area (excluding ruler). Keep sizes moderate.
    const rulerH = 18;
    const waveH = Math.max(10, Math.floor((wrapSize.h || el.clientHeight) - (rulerH + 2)));
    const reqW = clamp(Math.floor(wrapSize.w || el.clientWidth), 160, 1400);
    const reqH = clamp(Math.floor(Math.min(512, waveH)), 96, 512);
    const fftSize = spectrogram.fftSize || 2048;

    const key = `${fileId}|${fftSize}|${view.start.toFixed(5)}|${view.end.toFixed(5)}|${reqW}x${reqH}`;
    if (specKeyRef.current === key) return;
    specKeyRef.current = key;

    const seq = ++specReqSeqRef.current;
    const run = async () => {
      const tile = await invoke<SpectrogramTileResponse>('spectrogram.getTile', {
        id: fileId,
        startNorm: view.start,
        endNorm: view.end,
        width: reqW,
        height: reqH,
        fftSize,
      });
      if (seq !== specReqSeqRef.current) return;
      if (!tile || !tile.rgba || tile.w <= 0 || tile.h <= 0) return;

      // Decode base64 RGBA
      const bin = atob(tile.rgba);
      const bytes = new Uint8Array(bin.length);
      for (let i = 0; i < bin.length; i++) bytes[i] = bin.charCodeAt(i);

      const off = document.createElement('canvas');
      off.width = tile.w;
      off.height = tile.h;
      const octx = off.getContext('2d');
      if (!octx) return;
      const img = new ImageData(new Uint8ClampedArray(bytes.buffer), tile.w, tile.h);
      octx.putImageData(img, 0, 0);
      specOffscreenRef.current = off;
    };
    void run();
  }, [fileId, spectrogram?.enabled, spectrogram?.fftSize, transport.loaded, view.end, view.start, wrapSize.h, wrapSize.w]);

  const renderBaseLayer = () => {
    const wrap = wrapRef.current;
    if (!wrap) return;
    const w = Math.max(10, Math.floor(wrap.clientWidth));
    const h = Math.max(10, Math.floor(wrap.clientHeight));

    const rulerH = 18;
    const waveTop = rulerH + 2;
    const waveH = h - waveTop;

    const specEnabled = !!spectrogram?.enabled;
    const specSplit = !!spectrogram?.split;
    const specBlend = clamp(spectrogram?.blend ?? 0.5, 0, 1);
    const spectHeight = specEnabled && specSplit ? Math.max(0, Math.floor(waveH * specBlend)) : waveH;
    const waveDrawTop = specEnabled && specSplit ? waveTop + spectHeight : waveTop;
    const waveDrawH = specEnabled && specSplit ? Math.max(0, waveH - spectHeight) : waveH;
    const midY = waveDrawTop + waveDrawH * 0.5;

    const dataKey = data ? `${data.peaks.length}:${data.rms?.length ?? 0}` : 'none';
    const specKey = specEnabled ? `${specSplit ? 'S' : 'M'}:${specBlend.toFixed(3)}:${spectrogram?.fftSize ?? 0}` : 'off';
    const key = `${w}x${h}@${dpr}|${view.start.toFixed(5)}-${view.end.toFixed(5)}|z=${view.z.toFixed(2)}|${specKey}|in=${transport.inPoint.toFixed(4)}|out=${transport.outPoint.toFixed(4)}|${dataKey}|file=${fileId ?? 0}`;

    if (baseLayerRef.current?.key === key) return;

    const base = document.createElement('canvas');
    base.width = Math.floor(w * dpr);
    base.height = Math.floor(h * dpr);
    const bctx = base.getContext('2d');
    if (!bctx) return;
    bctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    bctx.clearRect(0, 0, w, h);
    bctx.imageSmoothingEnabled = true;

    // Background grid + ruler
    bctx.save();
    bctx.strokeStyle = 'rgba(120,170,255,0.08)';
    bctx.lineWidth = 1;
    const viewDur = (transport.duration || 0) * view.win;
    const step = niceStep(Math.max(0.001, viewDur / 7));
    if (transport.duration > 0) {
      const startSec = view.start * transport.duration;
      const endSec = view.end * transport.duration;
      const first = Math.ceil(startSec / step) * step;
      for (let t = first; t <= endSec + 1e-6; t += step) {
        const norm = t / transport.duration;
        const x = ((norm - view.start) / view.win) * w;
        bctx.beginPath();
        bctx.moveTo(x + 0.5, 0);
        bctx.lineTo(x + 0.5, h);
        bctx.stroke();
      }
    }
    bctx.strokeStyle = 'rgba(200,220,255,0.06)';
    bctx.beginPath();
    bctx.moveTo(0, midY + 0.5);
    bctx.lineTo(w, midY + 0.5);
    bctx.stroke();
    bctx.restore();

    bctx.save();
    bctx.fillStyle = 'rgba(170,195,235,0.75)';
    bctx.font = '10px ui-monospace, SFMono-Regular, Menlo, Consolas, monospace';
    if (transport.duration > 0) {
      const viewDur = (transport.duration || 0) * view.win;
      const step = niceStep(Math.max(0.001, viewDur / 7));
      const startSec = view.start * transport.duration;
      const endSec = view.end * transport.duration;
      const first = Math.ceil(startSec / step) * step;
      for (let t = first; t <= endSec + 1e-6; t += step) {
        const norm = t / transport.duration;
        const x = ((norm - view.start) / view.win) * w;
        bctx.fillText(fmtTime(t), x + 2, 12);
      }
    }
    bctx.restore();

    // Spectrogram (base layer)
    if (specEnabled && transport.loaded) {
      const off = specOffscreenRef.current;
      if (off) {
        if (specSplit) {
          if (spectHeight > 10) {
            bctx.save();
            bctx.globalAlpha = 1.0;
            bctx.drawImage(off, 0, waveTop, w, spectHeight);
            bctx.restore();
          }
        } else {
          let spectAlpha = specBlend;
          if (view.z >= 2.0) spectAlpha = Math.min(spectAlpha, 0.15);
          if (spectAlpha > 0.01) {
            bctx.save();
            bctx.globalAlpha = spectAlpha;
            bctx.drawImage(off, 0, waveTop, w, waveH);
            bctx.restore();
          }
        }
      }
    }

    // Shade outside region
    if (hasRegion && transport.duration > 0) {
      const inPt = clamp(transport.inPoint, 0, 1);
      const outPt = clamp(transport.outPoint, 0, 1);
      const xIn = ((inPt - view.start) / view.win) * w;
      const xOut = ((outPt - view.start) / view.win) * w;
      bctx.save();
      bctx.fillStyle = 'rgba(0,0,0,0.38)';
      if (inPt > view.start) bctx.fillRect(0, waveTop, clamp(xIn, 0, w), waveH);
      else bctx.fillRect(0, waveTop, w, waveH);
      if (outPt < view.end) bctx.fillRect(clamp(xOut, 0, w), waveTop, w - clamp(xOut, 0, w), waveH);
      bctx.restore();
    }

    // Waveform (smooth path)
    if (data && data.peaks.length > 1 && waveDrawH > 8) {
      const peaks = data.peaks;
      const frames = peaks.length;
      const startFrame = Math.floor(view.start * (frames - 1));
      const endFrame = Math.max(startFrame + 1, Math.floor(view.end * (frames - 1)));
      const visible = Math.max(1, endFrame - startFrame);

      const volScale = clamp(transport.volume ?? 1.0, 0.0, 1.5);
      let waveAlpha = 1.0;
      if (specEnabled && !specSplit) {
        let spectAlpha = specBlend;
        if (view.z >= 2.0) spectAlpha = Math.min(spectAlpha, 0.15);
        waveAlpha = 1.0 - spectAlpha;
      }

      // Build envelope per pixel for smooth edge.
      const env = new Float32Array(w);
      for (let x = 0; x < w; x++) {
        const t0 = w <= 1 ? 0 : x / (w - 1);
        const t1 = w <= 1 ? 1 : (x + 1) / (w - 1);
        const f0 = startFrame + Math.floor(t0 * visible);
        const f1 = startFrame + Math.floor(t1 * visible);
        let p = 0;
        const a = clamp(f0, 0, frames - 1);
        const b = clamp(Math.max(a + 1, f1), 0, frames);
        for (let f = a; f < b; f++) p = Math.max(p, Math.abs(peaks[f] || 0));
        env[x] = clamp(p * volScale, 0, 1);
      }

      bctx.save();
      bctx.globalAlpha = clamp(waveAlpha, 0, 1);

      // Fill body (soft)
      bctx.fillStyle = 'rgba(80,160,255,0.20)';
      bctx.beginPath();
      bctx.moveTo(0, midY);
      for (let x = 0; x < w; x++) {
        const hh = env[x] * (waveDrawH * 0.85) * 0.5;
        bctx.lineTo(x + 0.5, midY - hh);
      }
      for (let x = w - 1; x >= 0; x--) {
        const hh = env[x] * (waveDrawH * 0.85) * 0.5;
        bctx.lineTo(x + 0.5, midY + hh);
      }
      bctx.closePath();
      bctx.fill();

      // Stroke edge (crisp)
      bctx.strokeStyle = 'rgba(120,200,255,0.70)';
      bctx.lineWidth = 1.25;
      bctx.beginPath();
      for (let x = 0; x < w; x++) {
        const hh = env[x] * (waveDrawH * 0.85) * 0.5;
        const y = midY - hh;
        if (x === 0) bctx.moveTo(x + 0.5, y);
        else bctx.lineTo(x + 0.5, y);
      }
      bctx.stroke();

      bctx.restore();
    }

    // Selected region overlay (glass)
    if (hasRegion) {
      const inPt = clamp(transport.inPoint, 0, 1);
      const outPt = clamp(transport.outPoint, 0, 1);
      const x0 = clamp(((inPt - view.start) / view.win) * w, 0, w);
      const x1 = clamp(((outPt - view.start) / view.win) * w, 0, w);
      const mid = waveTop + waveH * 0.5;
      bctx.save();
      const gTop = bctx.createLinearGradient(0, waveTop, 0, mid);
      gTop.addColorStop(0, 'rgba(80,160,255,0.16)');
      gTop.addColorStop(1, 'rgba(60,120,200,0.10)');
      bctx.fillStyle = gTop;
      bctx.fillRect(x0, waveTop, x1 - x0, waveH * 0.5);
      const gBot = bctx.createLinearGradient(0, mid, 0, waveTop + waveH);
      gBot.addColorStop(0, 'rgba(60,120,200,0.10)');
      gBot.addColorStop(1, 'rgba(40,90,160,0.08)');
      bctx.fillStyle = gBot;
      bctx.fillRect(x0, mid, x1 - x0, waveH * 0.5);
      bctx.strokeStyle = 'rgba(90,180,255,0.55)';
      bctx.lineWidth = 1;
      bctx.strokeRect(x0 + 0.5, waveTop + 0.5, (x1 - x0) - 1, waveH - 1);
      bctx.restore();
    }

    baseLayerRef.current = { key, canvas: base };
  };

  const drawFrame = (ctx: CanvasRenderingContext2D) => {
    const wrap = wrapRef.current;
    if (!wrap) return;
    const w = Math.max(10, Math.floor(wrap.clientWidth));
    const h = Math.max(10, Math.floor(wrap.clientHeight));
    const rulerH = 18;
    const waveTop = rulerH + 2;
    const waveH = h - waveTop;

    ctx.imageSmoothingEnabled = true;
    ctx.clearRect(0, 0, w, h);

    // Base layer
    const base = baseLayerRef.current?.canvas;
    if (base) {
      ctx.drawImage(base, 0, 0, w, h);
    }

    // Smooth playhead position
    const s = transportSampleRef.current;
    let pred = s.position;
    if (s.playing && s.duration > 0) {
      const dt = (performance.now() - s.tMs) / 1000;
      pred = clamp(s.position + dt / s.duration, 0, 1);
    }
    // Low-pass to kill any small jitter from polling.
    const prev = smoothPlayheadRef.current;
    const a = 0.22;
    const smooth = prev + (pred - prev) * a;
    smoothPlayheadRef.current = smooth;

    if (transport.loaded) {
      const x = ((smooth - view.start) / view.win) * w;
      if (x >= 0 && x <= w) {
        ctx.save();
        ctx.strokeStyle = 'rgba(255,210,80,0.95)';
        ctx.lineWidth = 2;
        ctx.beginPath();
        ctx.moveTo(x + 0.5, waveTop);
        ctx.lineTo(x + 0.5, waveTop + waveH);
        ctx.stroke();
        ctx.restore();
      }
    }

    // Zoom anchor marker (click-set)
    if (anchorRef.current.set) {
      const aPos = anchorRef.current.value;
      const x = ((aPos - view.start) / view.win) * w;
      if (x >= 0 && x <= w) {
        ctx.save();
        ctx.strokeStyle = 'rgba(255,220,90,0.55)';
        ctx.lineWidth = 1;
        ctx.beginPath();
        ctx.moveTo(x + 0.5, 0);
        ctx.lineTo(x + 0.5, h);
        ctx.stroke();
        ctx.restore();
      }
    }
  };

  // RAF render loop: keep playhead continuous even when backend only updates ~8Hz.
  useEffect(() => {
    const canvas = canvasRef.current;
    const wrap = wrapRef.current;
    if (!canvas || !wrap) return;

    let raf = 0;
    let alive = true;

    const ensureCanvasSize = () => {
      const w = Math.max(10, Math.floor(wrap.clientWidth));
      const h = Math.max(10, Math.floor(wrap.clientHeight));
      const pxW = Math.floor(w * dpr);
      const pxH = Math.floor(h * dpr);
      if (canvas.width !== pxW || canvas.height !== pxH) {
        canvas.width = pxW;
        canvas.height = pxH;
        canvas.style.width = `${w}px`;
        canvas.style.height = `${h}px`;
      }
      const ctx = canvas.getContext('2d');
      if (!ctx) return null;
      ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
      return ctx;
    };

    const tick = () => {
      if (!alive) return;
      renderBaseLayer();
      const ctx = ensureCanvasSize();
      if (ctx) drawFrame(ctx);

      // Render continuously while playing; otherwise render at a lower cadence (still catches hover/zoom changes).
      const dr = dragRef.current;
      const needsFullRate = transport.playing || dr.mode !== 'none';
      if (needsFullRate) {
        raf = requestAnimationFrame(tick);
      } else {
        raf = window.setTimeout(() => requestAnimationFrame(tick), 80) as unknown as number;
      }
    };

    tick();
    return () => {
      alive = false;
      cancelAnimationFrame(raf);
      window.clearTimeout(raf);
    };
  }, [
    dpr,
    data,
    fileId,
    hasRegion,
    spectrogram?.blend,
    spectrogram?.enabled,
    spectrogram?.fftSize,
    spectrogram?.split,
    transport.duration,
    transport.inPoint,
    transport.loaded,
    transport.outPoint,
    transport.playing,
    transport.volume,
    view.end,
    view.start,
    view.win,
    view.z,
    wrapSize.h,
    wrapSize.w,
  ]);

  // Wheel for zoom/pan (capture + prevent page scroll)
  useEffect(() => {
    const wrap = wrapRef.current;
    if (!wrap) return;
    const onWheel = (e: WheelEvent) => {
      if (!wrapRef.current) return;
      e.preventDefault();
      const norm = mouseXToNorm(e.clientX);

      // Shift + Wheel: spectrogram blend (ImGui behavior) - only when spectrogram visible and Ctrl not held
      if (spectrogram?.enabled && e.shiftKey && !e.ctrlKey) {
        const delta = e.deltaY < 0 ? 0.1 : -0.1;
        const next = clamp((spectrogram.blend ?? 0.5) + delta, 0, 1);
        onSpectrogramBlendChange?.(next);
        return;
      }

      if (e.ctrlKey) {
        const anchor = anchorRef.current.set ? anchorRef.current.value : norm;
        const factor = e.deltaY < 0 ? 1.15 : 1 / 1.15;
        setZoom((z0) => {
          const z1 = clamp(z0 * factor, 1.0, 20.0);
          if (z1 <= 1.0) {
            setViewCenter(0.5);
            anchorRef.current = { set: false, value: 0 };
          } else {
            setViewCenter(anchor);
          }
          return z1;
        });
        return;
      }

      // plain wheel: pan when zoomed
      if (zoom > 1.0) {
        const pan = (e.deltaY / 1000) * (0.9 / zoom);
        setViewCenter((c) => clamp(c + pan, 0, 1));
      }
    };
    wrap.addEventListener('wheel', onWheel, { passive: false });
    return () => wrap.removeEventListener('wheel', onWheel as any);
  }, [onSpectrogramBlendChange, spectrogram?.blend, spectrogram?.enabled, view.start, view.win, zoom]);

  const sendRegion = (a: number, b: number) => {
    // throttle via rAF (latest wins)
    const dr = dragRef.current;
    dr.latestA = a;
    dr.latestB = b;
    if (dr.rafPending) return;
    dr.rafPending = true;
    requestAnimationFrame(() => {
      dr.rafPending = false;
      const x0 = clamp(Math.min(dr.latestA, dr.latestB), 0, 1);
      const x1 = clamp(Math.max(dr.latestA, dr.latestB), 0, 1);
      void invoke('transport.setRegion', { inPoint: x0, outPoint: x1 });
    });
  };

  const onPointerDown = (e: React.PointerEvent) => {
    const wrap = wrapRef.current;
    if (!wrap) return;
    if (!transport.loaded) return;

    wrap.setPointerCapture?.(e.pointerId);
    const norm = mouseXToNorm(e.clientX);
    anchorRef.current = { set: true, value: norm };

    const r = wrap.getBoundingClientRect();
    const y = e.clientY - r.top;
    const isTopHalf = y < r.height * 0.5;
    const inSelectedRegion = hasRegion && norm >= transport.inPoint && norm <= transport.outPoint;

    // Right / middle click shortcuts
    if (e.button === 2) {
      e.preventDefault();
      void invoke('transport.setOutPoint', { value: norm });
      return;
    }
    if (e.button === 1 || (e.button === 0 && e.altKey)) {
      e.preventDefault();
      void invoke('transport.setInPoint', { value: norm });
      return;
    }
    if (e.button !== 0) return;

    // Ctrl+double click: reset zoom (ImGui parity)
    if (e.ctrlKey && (e.detail >= 2)) {
      e.preventDefault();
      setZoom(1.0);
      setViewCenter(0.5);
      anchorRef.current = { set: false, value: 0 };
      return;
    }

    const dr = dragRef.current;
    dr.startNorm = norm;
    dr.startX = e.clientX;
    dr.startY = e.clientY;
    dr.pendingSeek = false;

    if (isTopHalf) {
      // Click top half: optionally clear region if clicking outside, then seek+play on release.
      // ImGui: in split mode, clicking top half clears region unconditionally (to avoid conflict).
      if (hasRegion && (spectrogram?.split || !inSelectedRegion)) {
        void invoke('transport.clearInOut', {});
      }
      dr.mode = 'export';
      dr.pendingSeek = true;
      return;
    }

    // Bottom half: region selection drag
    dr.mode = 'region';
    sendRegion(norm, norm);
  };

  const onPointerMove = (e: React.PointerEvent) => {
    const wrap = wrapRef.current;
    if (!wrap) return;
    const dr = dragRef.current;
    if (dr.mode === 'none') return;

    const norm = mouseXToNorm(e.clientX);
    if (dr.mode === 'region') {
      sendRegion(dr.startNorm, norm);
      return;
    }

    if (dr.mode === 'export') {
      const dx = e.clientX - dr.startX;
      const dy = e.clientY - dr.startY;
      const dist = Math.sqrt(dx * dx + dy * dy);
      if (dist > 3.0) {
        dr.mode = 'none';
        dr.pendingSeek = false;
        void invoke('waveform.startRegionDrag', {});
      }
    }
  };

  const onPointerUp = (e: React.PointerEvent) => {
    const wrap = wrapRef.current;
    if (!wrap) return;
    const dr = dragRef.current;
    if (dr.mode === 'export' && dr.pendingSeek) {
      // ImGui 逻辑 (main.cpp:14542-14544):
      //   state.audioEngine.seekToPosition(exportStartPos);
      //   if (!state.audioEngine.isPlaying()) state.audioEngine.play();
      // 必须先等 seek 完成，再有条件 play
      const seekPos = dr.startNorm;
      void invoke('transport.seek', { position: seekPos }).then(() => {
        if (!transport.playing) {
          void invoke('transport.play', {});
        }
      });
    }
    dr.mode = 'none';
    dr.pendingSeek = false;
    wrap.releasePointerCapture?.(e.pointerId);
  };

  return (
    <div
      ref={wrapRef}
      className={className ? `mw-wrap ${className}` : 'mw-wrap'}
      onPointerDown={onPointerDown}
      onPointerMove={onPointerMove}
      onPointerUp={onPointerUp}
      onContextMenu={(e) => {
        // Keep right-click usable for setting out-point, and prevent browser menu.
        e.preventDefault();
      }}
      title={
        '上半区：单击跳转+播放；拖动→拖入DAW（有选区则导出选区）\n' +
        '下半区：拖动框选选区\n' +
        'Ctrl+滚轮：缩放；滚轮：平移（缩放后）\n' +
        '右键：设置 Out 点；Alt+左键/中键：设置 In 点；Ctrl+双击：重置缩放'
      }
    >
      <canvas ref={canvasRef} className="mw-canvas" />
    </div>
  );
}


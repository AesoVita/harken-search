/**
 * MiniWaveform - 文件列表行内小波形
 * 对标 ImGui DrawMiniWaveform (main.cpp:6044-6094)
 *   - peaks 外层半透明轮廓
 *   - rms 内层不透明实体
 *   - 当前播放文件显示红色播放头线
 */
import React, { useEffect, useRef, useState, memo } from 'react';
import { invoke } from '../../bridge';

interface MiniWaveformProps {
  fileId: number;
  width: number;
  height: number;
  isPlaying: boolean;
  playPos: number; // 0..1, only meaningful when isPlaying
  renderActive: boolean;
}

// 全局行级波形缓存：fileId → { peaks, rms }
const miniCache = new Map<number, { peaks: number[]; rms: number[] }>();
// 正在加载中的 fileId 集合
const loadingSet = new Set<number>();
const MINI_CACHE_MAX = 256; // avoid unbounded growth when scrolling large lists

function miniCacheSet(fileId: number, wf: { peaks: number[]; rms: number[] }) {
  // LRU-ish: refresh insertion order
  if (miniCache.has(fileId)) miniCache.delete(fileId);
  miniCache.set(fileId, wf);
  while (miniCache.size > MINI_CACHE_MAX) {
    const oldestKey = miniCache.keys().next().value as number | undefined;
    if (oldestKey === undefined) break;
    miniCache.delete(oldestKey);
  }
}

const MiniWaveform = memo(function MiniWaveform({ fileId, width, height, isPlaying, playPos, renderActive }: MiniWaveformProps) {
  const canvasRef = useRef<HTMLCanvasElement | null>(null);
  const rafRef = useRef(0);
  const [cacheRevision, setCacheRevision] = useState(0);

  useEffect(() => {
    if (!renderActive) return;
    let alive = true;
    // 如果还没有缓存且没在加载中，就发起 RPC
    if (!miniCache.has(fileId) && !loadingSet.has(fileId)) {
      loadingSet.add(fileId);
      void invoke<{ peaks: number[]; rms: number[] }>('waveform.getMini', { id: fileId }).then((wf) => {
        loadingSet.delete(fileId);
        if (wf && wf.peaks && wf.peaks.length > 0) {
          miniCacheSet(fileId, wf);
          if (alive) setCacheRevision((value) => value + 1);
        }
      }).catch(() => {
        loadingSet.delete(fileId);
      });
    }
    return () => {
      alive = false;
    };
  }, [fileId, renderActive]);

  useEffect(() => {
    if (!renderActive) return;
    const draw = () => {
      const canvas = canvasRef.current;
      if (!canvas) return;
      const dpr = window.devicePixelRatio || 1;
      const w = width;
      const h = height;
      const pxW = Math.floor(w * dpr);
      const pxH = Math.floor(h * dpr);
      if (canvas.width !== pxW || canvas.height !== pxH) {
        canvas.width = pxW;
        canvas.height = pxH;
      }
      const ctx = canvas.getContext('2d');
      if (!ctx) return;
      ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
      ctx.clearRect(0, 0, w, h);

      const cached = miniCache.get(fileId);
      if (!cached || cached.peaks.length === 0) {
        // 没有数据：画一条中线占位
        ctx.strokeStyle = 'rgba(120,160,255,0.12)';
        ctx.lineWidth = 1;
        ctx.beginPath();
        ctx.moveTo(0, h / 2 + 0.5);
        ctx.lineTo(w, h / 2 + 0.5);
        ctx.stroke();
        return;
      }

      const { peaks, rms } = cached;
      const numPeaks = peaks.length;
      const centerY = h / 2;

      // ImGui 逻辑 (main.cpp:6054-6090):
      // peaksToShow = min(numPeaks, width)
      // peaksPerPixel = numPeaks / peaksToShow
      // barWidth = max(1, pixelsPerPeak - 0.5)
      const peaksToShow = Math.min(numPeaks, Math.floor(w));
      const pixelsPerPeak = w / peaksToShow;
      const peaksPerPixel = numPeaks / peaksToShow;
      const barGap = 0.5;
      const barWidth = Math.max(1, pixelsPerPeak - barGap);
      const hasRms = rms.length === numPeaks;

      for (let i = 0; i < peaksToShow; i++) {
        let srcIndex = Math.floor(i * peaksPerPixel);
        if (srcIndex >= numPeaks) srcIndex = numPeaks - 1;

        const x = i * pixelsPerPeak;
        let peakH = Math.abs(peaks[srcIndex]) * h * 0.92;
        if (peakH < 1) peakH = 1;

        // 层1: Peak 外层 (半透明)
        ctx.fillStyle = 'rgba(80,160,255,0.47)';
        ctx.fillRect(x, centerY - peakH / 2, barWidth, peakH);

        // 层2: RMS 内层 (不透明实体)
        if (hasRms) {
          const rmsH = Math.abs(rms[srcIndex]) * h * 0.75;
          if (rmsH >= 0.5) {
            ctx.fillStyle = 'rgba(80,160,255,0.85)';
            ctx.fillRect(x, centerY - rmsH / 2, barWidth, rmsH);
          }
        }
      }

      // 播放头 (红线，和 ImGui 一致)
      if (isPlaying && playPos >= 0) {
        const px = playPos * w;
        ctx.strokeStyle = 'rgba(255,100,100,0.86)';
        ctx.lineWidth = 1.5;
        ctx.beginPath();
        ctx.moveTo(px + 0.5, 0);
        ctx.lineTo(px + 0.5, h);
        ctx.stroke();
      }
    };

    draw();

    // 如果正在播放则持续刷新播放头
    if (isPlaying) {
      let alive = true;
      const tick = () => {
        if (!alive) return;
        draw();
        rafRef.current = requestAnimationFrame(tick);
      };
      rafRef.current = requestAnimationFrame(tick);
      return () => {
        alive = false;
        cancelAnimationFrame(rafRef.current);
      };
    }
  }, [fileId, width, height, isPlaying, playPos, renderActive, cacheRevision]);

  return (
    <canvas
      ref={canvasRef}
      style={{ width: `${width}px`, height: `${height}px`, display: 'block' }}
    />
  );
});

export default MiniWaveform;

import React, { useEffect, useMemo, useRef, useState } from 'react';
import type { AnnotationSession, AnnotationZoneDocument } from './annotation-types';
import './desktop-annotation.css';

const DEFAULT_BRIDGE_PORT = 17891;
const POLL_INTERVAL_MS = 250;

function resolveBridgeOrigin() {
  const rawPort = Number(new URLSearchParams(window.location.search).get('bridge'));
  const port = Number.isInteger(rawPort) && rawPort > 0 && rawPort <= 65535
    ? rawPort
    : DEFAULT_BRIDGE_PORT;
  return `http://127.0.0.1:${port}`;
}

export default function DesktopAnnotationPage() {
  const bridgeOrigin = useMemo(resolveBridgeOrigin, []);
  const stageRef = useRef<HTMLDivElement>(null);
  const [session, setSession] = useState<AnnotationSession | null>(null);
  const [zoneDocument, setZoneDocument] = useState<AnnotationZoneDocument | null>(null);
  const [stageSize, setStageSize] = useState({ width: 0, height: 0 });
  const [connectionError, setConnectionError] = useState('');

  useEffect(() => {
    const stage = stageRef.current;
    if (!stage) return undefined;
    const update = () => setStageSize({ width: stage.clientWidth, height: stage.clientHeight });
    update();
    const observer = new ResizeObserver(update);
    observer.observe(stage);
    return () => observer.disconnect();
  }, []);

  useEffect(() => {
    let active = true;
    const poll = async () => {
      try {
        const [sessionResponse, zonesResponse] = await Promise.all([
          fetch(`${bridgeOrigin}/api/session`, { cache: 'no-store' }),
          fetch(`${bridgeOrigin}/api/zones`, { cache: 'no-store' }),
        ]);
        if (!sessionResponse.ok || !zonesResponse.ok) throw new Error('Annotation bridge unavailable');
        const nextSession = await sessionResponse.json() as AnnotationSession;
        const nextZones = await zonesResponse.json() as AnnotationZoneDocument;
        if (!active) return;
        setSession(nextSession);
        setZoneDocument(nextZones);
        setConnectionError('');
      } catch (error) {
        if (active) setConnectionError(error instanceof Error ? error.message : 'Annotation bridge unavailable');
      }
    };
    void poll();
    const timer = window.setInterval(() => void poll(), POLL_INTERVAL_MS);
    return () => {
      active = false;
      window.clearInterval(timer);
    };
  }, [bridgeOrigin]);

  const frame = session?.frame;
  const frameWidth = frame?.width ?? 0;
  const frameHeight = frame?.height ?? 0;
  const scale = frameWidth > 0 && frameHeight > 0
    ? Math.min(stageSize.width / frameWidth, stageSize.height / frameHeight)
    : 0;
  const displayWidth = frameWidth * scale;
  const displayHeight = frameHeight * scale;
  const offsetX = (stageSize.width - displayWidth) * 0.5;
  const offsetY = (stageSize.height - displayHeight) * 0.5;
  const synchronized = Boolean(session?.synchronized && zoneDocument?.synchronized);
  const zones = synchronized
    ? [...(zoneDocument?.zones ?? [])].sort((left, right) => right.area - left.area)
    : [];

  const status = connectionError
    ? connectionError
    : !session?.frame.available
      ? 'Waiting for Harken Search'
      : !session.zoneExportAvailable
        ? 'Zone export disabled'
        : !synchronized
          ? 'Waiting for synchronized frame'
          : 'Connected to real Harken Search window';

  return <main className="desktop-annotation-page">
    <header className="desktop-annotation-status">
      <span className={synchronized ? 'connected' : 'waiting'} />
      <b>Harken Search Desktop Annotation</b>
      <em>{status}</em>
      {session?.frame.ageSeconds != null && <small>{session.frame.ageSeconds.toFixed(1)}s</small>}
    </header>
    <div className="desktop-annotation-stage" ref={stageRef}>
      {frame?.available && <img
        className="desktop-annotation-frame"
        src={`${bridgeOrigin}/frame.jpg?sequence=${frame.sequence}`}
        style={{ left: offsetX, top: offsetY, width: displayWidth, height: displayHeight }}
        alt="Live capture of the real Harken Search desktop client"
        draggable={false}
      />}
      {zones.map(zone => <button
        type="button"
        className="desktop-annotation-zone"
        id={zone.stableId}
        data-annotation-zone="true"
        data-zone-key={zone.key}
        data-source-anchor={zone.sourceAnchor}
        aria-label={zone.displayName || zone.key}
        title={`${zone.displayName || zone.key}${zone.sourceAnchor ? ` · ${zone.sourceAnchor}` : ''}`}
        style={{
          left: offsetX + zone.minX * scale,
          top: offsetY + zone.minY * scale,
          width: Math.max(1, (zone.maxX - zone.minX) * scale),
          height: Math.max(1, (zone.maxY - zone.minY) * scale),
        }}
      />)}
      {!frame?.available && <div className="desktop-annotation-empty">Launch Harken Search in annotation mode to begin.</div>}
    </div>
  </main>;
}

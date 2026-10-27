import React, { useEffect, useRef, useState } from 'react';

type Props = { style: React.CSSProperties };

const mainWave = 'M0 65 L20 63 25 52 30 76 35 35 40 91 45 51 50 82 55 20 60 100 65 44 70 88 76 28 82 102 90 42 98 86 106 19 114 96 122 35 130 82 140 29 150 91 160 38 170 74 180 48 190 66 205 42 220 72 235 49 250 64 270 53 300 65 340 60 380 66 430 62 500 65 620 64 760 65 900 64 1100 65 1500 64 1900 65 2380 64';
const times = ['00:00:00.00', '00:00:00.57', '00:00:01.15', '00:00:01.73', '00:00:02.31', '00:00:02.89', '00:00:03.46', '00:00:04.04', '00:00:04.62', '00:00:05.20'];
const TRANSPORT_DB_OPTIONS = ['JUSTMIXRFull'];
const TRANSPORT_HISTORY_ITEMS = [
  'pakchunk0-WindowsClient_005161_00000A4A1835',
  'pakchunk0-WindowsClient_005169_00000A4E7221',
  'pakchunk0-WindowsClient_005177_00000A537835',
  'pakchunk0-WindowsClient_005181_00000A55C092',
];
const TRANSPORT_DAW_TARGETS = [
  { name: 'REAPER', capability: 'Extension / pipe', status: 'ready' },
  { name: 'Pro Tools', capability: 'PTSL / fallback', status: 'idle' },
  { name: 'Cubase', capability: 'Direct spotting', status: 'idle' },
  { name: 'Nuendo', capability: 'SKI / direct spotting', status: 'ready' },
] as const;

const MONO_STEREO_TARGET_OFFSET = 5;
const MONO_STEREO_TARGET_SCALE = 0.72;
const MONO_STEREO_DECAY = 12;
const REVERSE_ANIMATION_SPEED = 5;
const CENTER_SENSE_DRAG_RATE = 0.006;
const VOLUME_WHEEL_STEP = 5;
const PAN_DRAG_RATE = 0.5;
const FOOTER_MODE_ANIMATION_SPEED = 5;
const FIXTURE_DURATION_SECONDS = 5.2;
const WAVE_AMPLITUDE_MIN = 0.1;
const WAVE_AMPLITUDE_MAX = 8;

function useExponentialAnimatedValue(target: number, rate: number, epsilon = 0.001) {
  const [value, setValue] = useState(target);
  const valueRef = useRef(target);
  const frameRef = useRef<number | null>(null);

  useEffect(() => {
    if (frameRef.current !== null) cancelAnimationFrame(frameRef.current);
    let previous = performance.now();

    const animate = (now: number) => {
      const dt = Math.min((now - previous) / 1000, 0.05);
      previous = now;
      const step = 1 - Math.exp(-rate * dt);
      const next = valueRef.current + (target - valueRef.current) * step;

      if (Math.abs(next - target) <= epsilon) {
        valueRef.current = target;
        setValue(target);
        frameRef.current = null;
        return;
      }

      valueRef.current = next;
      setValue(next);
      frameRef.current = requestAnimationFrame(animate);
    };

    if (Math.abs(valueRef.current - target) <= epsilon) {
      valueRef.current = target;
      setValue(target);
      return undefined;
    }

    frameRef.current = requestAnimationFrame(animate);
    return () => {
      if (frameRef.current !== null) cancelAnimationFrame(frameRef.current);
      frameRef.current = null;
    };
  }, [target, rate, epsilon]);

  return value;
}

function useLinearAnimatedValue(target: number, speed: number, epsilon = 0.001) {
  const [value, setValue] = useState(target);
  const valueRef = useRef(target);
  const frameRef = useRef<number | null>(null);

  useEffect(() => {
    if (frameRef.current !== null) cancelAnimationFrame(frameRef.current);
    let previous = performance.now();

    const animate = (now: number) => {
      const dt = Math.min((now - previous) / 1000, 0.05);
      previous = now;
      const delta = speed * dt;
      const distance = target - valueRef.current;

      if (Math.abs(distance) <= Math.max(epsilon, delta)) {
        valueRef.current = target;
        setValue(target);
        frameRef.current = null;
        return;
      }

      const next = valueRef.current + Math.sign(distance) * delta;
      valueRef.current = next;
      setValue(next);
      frameRef.current = requestAnimationFrame(animate);
    };

    if (Math.abs(valueRef.current - target) <= epsilon) {
      valueRef.current = target;
      setValue(target);
      return undefined;
    }

    frameRef.current = requestAnimationFrame(animate);
    return () => {
      if (frameRef.current !== null) cancelAnimationFrame(frameRef.current);
      frameRef.current = null;
    };
  }, [target, speed, epsilon]);

  return value;
}

function TransportGlyph({ name }: { name: 'meter' | 'dial' | 'wave' | 'prev' | 'next' | 'pause' | 'play' }) {
  const paths: Record<string, React.ReactNode> = {
    meter: <><path d="M3 12V8M6.5 12V5M10 12V3M13 12V7" /><path d="M2 13.5h12" /></>,
    dial: <><circle cx="8" cy="8" r="5.5" /><path d="M8 8 11.5 5M5 13h6" /></>,
    wave: <path d="M2 8c1.3-6 2.7 6 4 0s2.7-6 4 0 2.7 6 4 0" />,
    prev: <><path d="M4 3v10M13 4l-6 4 6 4z" /></>,
    next: <><path d="M12 3v10M3 4l6 4-6 4z" /></>,
    pause: <><path d="M5 3v10M11 3v10" /></>,
    play: <path d="m5 3 7 5-7 5z" />,
  };

  return <svg className="transport-glyph" viewBox="0 0 16 16" aria-hidden="true" fill="none" stroke="currentColor" strokeWidth="1.25" strokeLinecap="round" strokeLinejoin="round">{paths[name]}</svg>;
}

function TransportShuffleGlyph({ progress }: { progress: number }) {
  const topEndY = 5 + 6 * progress;
  const bottomEndY = 11 - 6 * progress;
  return <svg className="transport-glyph" viewBox="0 0 16 16" aria-hidden="true" fill="none" stroke="currentColor" strokeWidth="1.35" strokeLinecap="round" strokeLinejoin="round" data-source="src/main.cpp:53260-53295">
    <path d={`M2 5 C5 5 6 ${topEndY} 11 ${topEndY}`} opacity={1 - progress * 0.5} />
    <path d={`M2 11 C5 11 6 ${bottomEndY} 11 ${bottomEndY}`} />
    <path d={`M11 ${topEndY} 14 ${topEndY - 2}M11 ${topEndY} 14 ${topEndY + 2}`} />
    <path d={`M11 ${bottomEndY} 14 ${bottomEndY - 2}M11 ${bottomEndY} 14 ${bottomEndY + 2}`} />
  </svg>;
}

function TransportListeningGlyph({ mode }: { mode: 'browse' | 'audition' }) {
  return <svg className="transport-glyph" viewBox="0 0 16 16" aria-hidden="true" fill="none" stroke="currentColor" strokeWidth="1.35" strokeLinecap="round" strokeLinejoin="round" data-source="src/main.cpp:53298-53329">
    {mode === 'browse'
      ? <><path d="M3.5 9.5c0-5 9-5 9 0" /><path d="M2.5 8.5v4h2.5v-4M13.5 8.5v4H11v-4" /></>
      : <><path d="M2.5 4.5 6.5 8l-4 3.5Z" /><path d="M9.5 4.5 13.5 8l-4 3.5Z" /></>}
  </svg>;
}

function TransportStereoGlyph({ mono, offset, scale }: { mono: boolean; offset: number; scale: number }) {
  return <svg className="transport-source-glyph" viewBox="0 0 30 32" aria-hidden="true" data-source="src/main.cpp:50968-51010">
    <circle cx={15 - offset} cy="16" r={7 * scale} />
    <circle cx={15 + offset} cy="16" r={7 * scale} />
  </svg>;
}

function TransportReverseGlyph({ progress }: { progress: number }) {
  const forward = [21, 13, 8, 5, 3];
  const reverse = [3, 5, 8, 13, 21];

  return <svg className="transport-source-glyph" viewBox="0 0 36 32" aria-hidden="true" data-source="src/main.cpp:51012-51103">
    {forward.map((height, index) => {
      const interpolated = height + (reverse[index] - height) * progress;
      const scaled = (32 * 0.45 / 21) * interpolated;
      const x = 9.25 + index * 4;
      return <line key={index} x1={x} y1={24.96 - scaled} x2={x} y2="24.96" />;
    })}
    {progress > 0.02 && <>
      <path
        className="transport-reverse-swoosh"
        d="M24.75 7.04 C24.75 1.8 14.25 1.8 9.25 13.44"
        style={{ opacity: Math.min(1, progress * 1.4) }}
      />
      <path
        className="transport-reverse-swoosh"
        d="M9.25 13.44 L11.75 10.94"
        style={{ opacity: Math.min(1, progress * 1.4) }}
      />
    </>}
  </svg>;
}

function TransportPanGlyph({ value }: { value: number }) {
  const percentage = (value + 50) / 100;
  const angle = Math.PI + percentage * Math.PI;
  const dotX = 20 + Math.cos(angle) * 12;
  const dotY = 22 + Math.sin(angle) * 12;
  return <svg className="transport-source-glyph" viewBox="0 0 52 32" aria-hidden="true" data-source="src/main.cpp:29939-30060">
    <circle className="pan-plate" cx="20" cy="22" r="14" />
    <path className="pan-arc" d="M8 22 A12 12 0 0 1 32 22" />
    <line className="pan-tick" x1="20" y1="7" x2="20" y2="12" />
    {value !== 0 && <path className="pan-value-arc" d={value > 0
      ? `M20 10 A12 12 0 0 1 ${dotX} ${dotY}`
      : `M${dotX} ${dotY} A12 12 0 0 1 20 10`} />}
    <circle className="pan-dot" cx={dotX} cy={dotY} r="3" />
  </svg>;
}

function pointOnCenterSenseArc(radius: number, angle: number) {
  return { x: 20 + Math.cos(angle) * radius, y: 19.84 + Math.sin(angle) * radius };
}

function TransportCenterSenseGlyph({ value }: { value: number }) {
  const angle = Math.max(20, 150 + value * 120) * Math.PI / 180;
  const start = -Math.PI / 2 - angle / 2;
  const end = -Math.PI / 2 + angle / 2;
  const largeArc = angle > Math.PI ? 1 : 0;
  const outerStart = pointOnCenterSenseArc(14.08, start);
  const outerEnd = pointOnCenterSenseArc(14.08, end);
  const innerStart = pointOnCenterSenseArc(4, start);
  const innerEnd = pointOnCenterSenseArc(4, end);
  const fillPath = `M${outerStart.x} ${outerStart.y} A14.08 14.08 0 ${largeArc} 1 ${outerEnd.x} ${outerEnd.y} L${innerEnd.x} ${innerEnd.y} A4 4 0 ${largeArc} 0 ${innerStart.x} ${innerStart.y} Z`;
  const outlinePath = `M${outerStart.x} ${outerStart.y} A14.08 14.08 0 ${largeArc} 1 ${outerEnd.x} ${outerEnd.y} M${innerStart.x} ${innerStart.y} A4 4 0 ${largeArc} 1 ${innerEnd.x} ${innerEnd.y} M${outerStart.x} ${outerStart.y} L${innerStart.x} ${innerStart.y} M${outerEnd.x} ${outerEnd.y} L${innerEnd.x} ${innerEnd.y}`;

  return <svg className="transport-source-glyph" viewBox="0 0 52 32" aria-hidden="true" data-source="src/main.cpp:51146-51270">
    <path className="center-fill" d={fillPath} />
    <path className="center-outline" d={outlinePath} />
    <text x="38" y="30">BM</text>
  </svg>;
}

function TransportLoopButton({ active, onClick }: { active: boolean; onClick: () => void }) {
  return <button type="button" className={`transport-loop ${active ? 'active' : ''}`} onClick={onClick} data-source="src/main.cpp:29338-29420">
    <i className="loop-led" />
    <span>Loop</span>
  </button>;
}

function TransportDbSwitch({ value, options, onChange }: { value: string; options: string[]; onChange: (next: string) => void }) {
  const [open, setOpen] = useState(false);
  const shellRef = useRef<HTMLDivElement | null>(null);

  useEffect(() => {
    if (!open) return undefined;

    const handlePointerDown = (event: PointerEvent) => {
      if (shellRef.current?.contains(event.target as Node)) return;
      setOpen(false);
    };

    window.addEventListener('pointerdown', handlePointerDown);
    return () => window.removeEventListener('pointerdown', handlePointerDown);
  }, [open]);

  return <div ref={shellRef} className="transport-db-shell" data-source="src/main.cpp:51784-51833">
    <button
      type="button"
      className="transport-db-switch"
      aria-haspopup="listbox"
      aria-expanded={open}
      onClick={() => setOpen(current => !current)}>
      <span className="transport-db-switch-label">{value}</span>
      <span className="transport-db-switch-arrow" aria-hidden="true">▼</span>
    </button>
    {open && <div className="transport-db-popup" role="listbox">
      {options.map(option => <button
        key={option}
        type="button"
        role="option"
        aria-selected={option === value}
        className={`transport-db-option ${option === value ? 'current' : ''}`}
        onClick={() => {
          onChange(option);
          setOpen(false);
        }}>
        {option}
      </button>)}
    </div>}
  </div>;
}

function TransportDawWidget() {
  const [selectedIndex, setSelectedIndex] = useState(0);
  const [open, setOpen] = useState(false);
  const [showDawConfiguration, setShowDawConfiguration] = useState(false);
  const [dawExecutablePath, setDawExecutablePath] = useState('');
  const [dawProcessName, setDawProcessName] = useState('');
  const [dawRefreshFeedback, setDawRefreshFeedback] = useState('');
  const [dawStatuses, setDawStatuses] = useState(TRANSPORT_DAW_TARGETS.map(target => target.status as string));
  const [dawSending, setDawSending] = useState(false);
  const [toastVisible, setToastVisible] = useState(false);
  const shellRef = useRef<HTMLDivElement | null>(null);
  const selected = TRANSPORT_DAW_TARGETS[selectedIndex];
  const selectedStatus = dawStatuses[selectedIndex];

  useEffect(() => {
    if (!open) return undefined;
    const closeOutside = (event: PointerEvent) => {
      if (!shellRef.current?.contains(event.target as Node)) setOpen(false);
    };
    window.addEventListener('pointerdown', closeOutside);
    return () => window.removeEventListener('pointerdown', closeOutside);
  }, [open]);

  const send = () => {
    if (dawSending) return;
    setDawSending(true);
    setToastVisible(false);
    window.setTimeout(() => {
      setDawSending(false);
      setToastVisible(true);
      window.setTimeout(() => setToastVisible(false), 1400);
    }, 650);
  };
  const refreshDawStatus = () => {
    setOpen(false);
    setDawRefreshFeedback('Refreshing status...');
    setDawStatuses(current => current.map(() => 'detecting'));
    window.setTimeout(() => {
      setDawStatuses(TRANSPORT_DAW_TARGETS.map(target => target.status));
      setDawRefreshFeedback('Status refreshed');
      window.setTimeout(() => setDawRefreshFeedback(''), 1200);
    }, 500);
  };

  return <div ref={shellRef} className="daw-send-widget" data-source="src/DAWPanel.cpp:195-390">
    <button type="button" className="daw-send-main" onClick={send} disabled={dawSending}>
      {dawSending
        ? <span className="daw-send-spinner" aria-hidden="true"><i /><i /><i /></span>
        : <svg className="daw-plane" viewBox="0 0 16 16" aria-hidden="true"><path d="M2 8 14 2l-4.2 12-2.2-4.1L2 8Zm5.6 1.9L14 2" /></svg>}
      <span>Send to {selected.name}</span>
      <i className={`daw-status ${selectedStatus}`} />
    </button>
    <button type="button" className="daw-send-arrow" aria-label="Choose DAW" aria-expanded={open} onClick={() => setOpen(value => !value)}><svg viewBox="0 0 12 12" aria-hidden="true"><path d="m3 4.5 3 3 3-3" /></svg></button>
    {open && <div className="daw-send-popup" role="listbox">
      {TRANSPORT_DAW_TARGETS.map((target, index) => <button type="button" className={index === selectedIndex ? 'selected' : ''} role="option" aria-selected={index === selectedIndex} key={target.name} onClick={() => { setSelectedIndex(index); setOpen(false); }}>
        <i className={`daw-status ${dawStatuses[index]}`} />
        <span><b>{target.name}</b><small>{target.capability}</small></span>
        <em>Ctrl+Shift+{Math.min(index + 1, 3)}</em>
        {index === selectedIndex && <strong>✓</strong>}
      </button>)}
      <div className="daw-popup-divider" />
      <button type="button" className="daw-popup-action" onClick={() => { setOpen(false); setShowDawConfiguration(true); }}>Configure DAW...</button>
      <button type="button" className="daw-popup-action" onClick={refreshDawStatus}>Refresh Status</button>
    </div>}
    {showDawConfiguration && <div className="daw-config-panel" role="dialog" aria-label={`Configure ${selected.name}`} data-source="src/DAWPanel.cpp:356-380">
      <strong>Configure {selected.name}</strong>
      <label>Executable<input value={dawExecutablePath} onChange={event => setDawExecutablePath(event.target.value)} placeholder="Path to DAW executable" /></label>
      <label>Process<input value={dawProcessName} onChange={event => setDawProcessName(event.target.value)} placeholder="Process name" /></label>
      <div><button type="button" onClick={() => setShowDawConfiguration(false)}>Cancel</button><button type="button" onClick={() => setShowDawConfiguration(false)}>Save</button></div>
    </div>}
    {dawRefreshFeedback && <span className="daw-refresh-feedback" role="status">{dawRefreshFeedback}</span>}
    {toastVisible && <span className="daw-send-toast">Sent to {selected.name}</span>}
  </div>;
}

export default function RuntimeTransport({ style }: Props) {
  const [playing, setPlaying] = useState(true);
  const [looping, setLooping] = useState(false);
  const [normalized, setNormalized] = useState(false);
  const [mono, setMono] = useState(false);
  const [reversed, setReversed] = useState(false);
  const [volumePercent, setVolumePercent] = useState(116);
  const [panValue, setPanValue] = useState(0);
  const [centerSense, setCenterSense] = useState(0);
  const [shuffle, setShuffle] = useState(false);
  const [listeningMode, setListeningMode] = useState<'browse' | 'audition'>('browse');
  const [playbackProgress, setPlaybackProgress] = useState(0.006);
  const [waveformZoom, setWaveformZoom] = useState(1);
  const [waveformViewCenter, setWaveformViewCenter] = useState(0.5);
  const [waveformAmplitude, setWaveformAmplitude] = useState(1);
  const [dbName, setDbName] = useState('JUSTMIXRFull');
  const [playbackHistoryIndex, setPlaybackHistoryIndex] = useState(2);
  const volumePointerRef = useRef<number | null>(null);
  const panPointerRef = useRef<{ pointerId: number; x: number; y: number } | null>(null);
  const centerSensePointerRef = useRef<{ pointerId: number; x: number } | null>(null);
  const waveScrollPointerRef = useRef<{ pointerId: number; offsetX: number } | null>(null);
  const waveAmplitudePointerRef = useRef<number | null>(null);

  const stereoOffset = useExponentialAnimatedValue(mono ? 0 : MONO_STEREO_TARGET_OFFSET, MONO_STEREO_DECAY);
  const stereoScale = useExponentialAnimatedValue(mono ? 1 : MONO_STEREO_TARGET_SCALE, MONO_STEREO_DECAY);
  const reverseProgress = useLinearAnimatedValue(reversed ? 1 : 0, REVERSE_ANIMATION_SPEED);
  const footerShuffleProgress = useLinearAnimatedValue(shuffle ? 1 : 0, FOOTER_MODE_ANIMATION_SPEED);

  useEffect(() => {
    if (!playing) return undefined;
    let frame = 0;
    let previous = performance.now();
    const tick = (now: number) => {
      const dt = Math.min((now - previous) / 1000, 0.05);
      previous = now;
      setPlaybackProgress(current => {
        const next = current + dt / FIXTURE_DURATION_SECONDS;
        if (next >= 1) {
          if (looping) return next % 1;
          setPlaying(false);
          return 1;
        }
        return next;
      });
      frame = requestAnimationFrame(tick);
    };
    frame = requestAnimationFrame(tick);
    return () => cancelAnimationFrame(frame);
  }, [playing, looping]);

  const formatTransportTime = (progress: number) => {
    const total = Math.max(0, progress * FIXTURE_DURATION_SECONDS);
    const minutes = Math.floor(total / 60);
    const seconds = (total % 60).toFixed(2).padStart(5, '0');
    return `00:${String(minutes).padStart(2, '0')}:${seconds}`;
  };

  const handleWaveformSeek = (event: React.PointerEvent<HTMLDivElement>) => {
    const bounds = event.currentTarget.getBoundingClientRect();
    setPlaybackProgress(Math.max(0, Math.min(1, (event.clientX - bounds.left) / bounds.width)));
  };

  const handleWaveformWheel = (event: React.WheelEvent<HTMLDivElement>) => {
    if (!event.ctrlKey) return;
    event.preventDefault();
    const direction = Math.sign(-event.deltaY);
    if (direction !== 0) setWaveformZoom(current => Math.max(1, Math.min(4, current + direction * 0.25)));
  };

  const handleWaveScrollPointerDown = (event: React.PointerEvent<HTMLDivElement>) => {
    const bounds = event.currentTarget.getBoundingClientRect();
    const thumbWidth = bounds.width / waveformZoom;
    const thumbLeft = waveformViewCenter * (bounds.width - thumbWidth);
    const localX = event.clientX - bounds.left;
    const offsetX = localX >= thumbLeft && localX <= thumbLeft + thumbWidth ? localX - thumbLeft : thumbWidth / 2;
    waveScrollPointerRef.current = { pointerId: event.pointerId, offsetX };
    event.currentTarget.setPointerCapture(event.pointerId);
    setWaveformViewCenter(Math.max(0, Math.min(1, (localX - offsetX) / Math.max(1, bounds.width - thumbWidth))));
  };

  const handleWaveScrollPointerMove = (event: React.PointerEvent<HTMLDivElement>) => {
    const pointer = waveScrollPointerRef.current;
    if (!pointer || pointer.pointerId !== event.pointerId) return;
    const bounds = event.currentTarget.getBoundingClientRect();
    const thumbWidth = bounds.width / waveformZoom;
    const localX = event.clientX - bounds.left;
    setWaveformViewCenter(Math.max(0, Math.min(1, (localX - pointer.offsetX) / Math.max(1, bounds.width - thumbWidth))));
  };

  const releaseWaveScrollPointer = (event: React.PointerEvent<HTMLDivElement>) => {
    if (waveScrollPointerRef.current?.pointerId !== event.pointerId) return;
    waveScrollPointerRef.current = null;
    if (event.currentTarget.hasPointerCapture(event.pointerId)) event.currentTarget.releasePointerCapture(event.pointerId);
  };

  const updateWaveAmplitudeFromPointer = (clientY: number, element: HTMLDivElement) => {
    const bounds = element.getBoundingClientRect();
    const trackTop = 4;
    const trackBottom = bounds.height - 16;
    const t = 1 - Math.max(0, Math.min(1, (clientY - bounds.top - trackTop) / Math.max(1, trackBottom - trackTop)));
    setWaveformAmplitude(WAVE_AMPLITUDE_MIN + t * (WAVE_AMPLITUDE_MAX - WAVE_AMPLITUDE_MIN));
  };

  const handleWaveAmplitudePointerDown = (event: React.PointerEvent<HTMLDivElement>) => {
    waveAmplitudePointerRef.current = event.pointerId;
    event.currentTarget.setPointerCapture(event.pointerId);
    updateWaveAmplitudeFromPointer(event.clientY, event.currentTarget);
  };

  const handleWaveAmplitudePointerMove = (event: React.PointerEvent<HTMLDivElement>) => {
    if (waveAmplitudePointerRef.current !== event.pointerId) return;
    updateWaveAmplitudeFromPointer(event.clientY, event.currentTarget);
  };

  const releaseWaveAmplitudePointer = (event: React.PointerEvent<HTMLDivElement>) => {
    if (waveAmplitudePointerRef.current !== event.pointerId) return;
    waveAmplitudePointerRef.current = null;
    if (event.currentTarget.hasPointerCapture(event.pointerId)) event.currentTarget.releasePointerCapture(event.pointerId);
  };

  const handleWaveAmplitudeWheel = (event: React.WheelEvent<HTMLDivElement>) => {
    event.preventDefault();
    const step = event.shiftKey ? 0.05 : 0.2;
    setWaveformAmplitude(current => Math.max(WAVE_AMPLITUDE_MIN, Math.min(WAVE_AMPLITUDE_MAX, current + Math.sign(-event.deltaY) * step)));
  };

  const resetWaveAmplitude = (event: React.MouseEvent<HTMLDivElement>) => {
    event.preventDefault();
    setWaveformAmplitude(1);
  };

  const updateVolumeFromPointer = (clientX: number, element: HTMLDivElement) => {
    const bounds = element.getBoundingClientRect();
    const next = ((clientX - bounds.left - 2) / Math.max(1, bounds.width - 4)) * 120;
    setVolumePercent(Math.max(0, Math.min(120, next)));
  };

  const handleVolumePointerDown = (event: React.PointerEvent<HTMLDivElement>) => {
    if (event.ctrlKey) {
      setVolumePercent(100);
      return;
    }
    volumePointerRef.current = event.pointerId;
    event.currentTarget.setPointerCapture(event.pointerId);
    updateVolumeFromPointer(event.clientX, event.currentTarget);
  };

  const handleVolumePointerMove = (event: React.PointerEvent<HTMLDivElement>) => {
    if (volumePointerRef.current !== event.pointerId) return;
    updateVolumeFromPointer(event.clientX, event.currentTarget);
  };

  const releaseVolumePointer = (event: React.PointerEvent<HTMLDivElement>) => {
    if (volumePointerRef.current !== event.pointerId) return;
    volumePointerRef.current = null;
    if (event.currentTarget.hasPointerCapture(event.pointerId)) event.currentTarget.releasePointerCapture(event.pointerId);
  };

  const handleVolumeWheel = (event: React.WheelEvent<HTMLDivElement>) => {
    event.preventDefault();
    const direction = Math.sign(-event.deltaY);
    if (direction !== 0) setVolumePercent(current => Math.max(0, Math.min(120, current + direction * VOLUME_WHEEL_STEP)));
  };

  const resetVolumeFromContextMenu = (event: React.MouseEvent<HTMLDivElement>) => {
    event.preventDefault();
    setVolumePercent(100);
  };

  const handlePanPointerDown = (event: React.PointerEvent<HTMLButtonElement>) => {
    panPointerRef.current = { pointerId: event.pointerId, x: event.clientX, y: event.clientY };
    event.currentTarget.setPointerCapture(event.pointerId);
  };

  const handlePanPointerMove = (event: React.PointerEvent<HTMLButtonElement>) => {
    const pointer = panPointerRef.current;
    if (!pointer || pointer.pointerId !== event.pointerId) return;
    const dragDelta = (event.clientX - pointer.x) - (event.clientY - pointer.y);
    pointer.x = event.clientX;
    pointer.y = event.clientY;
    setPanValue(current => Math.max(-50, Math.min(50, current + dragDelta * PAN_DRAG_RATE)));
  };

  const releasePanPointer = (event: React.PointerEvent<HTMLButtonElement>) => {
    if (panPointerRef.current?.pointerId !== event.pointerId) return;
    panPointerRef.current = null;
    if (event.currentTarget.hasPointerCapture(event.pointerId)) event.currentTarget.releasePointerCapture(event.pointerId);
  };

  const resetPan = () => setPanValue(0);
  const resetPanFromContextMenu = (event: React.MouseEvent<HTMLButtonElement>) => {
    event.preventDefault();
    resetPan();
  };

  const handleCenterSensePointerDown = (event: React.PointerEvent<HTMLButtonElement>) => {
    centerSensePointerRef.current = { pointerId: event.pointerId, x: event.clientX };
    event.currentTarget.setPointerCapture(event.pointerId);
  };

  const handleCenterSensePointerMove = (event: React.PointerEvent<HTMLButtonElement>) => {
    const pointer = centerSensePointerRef.current;
    if (!pointer || pointer.pointerId !== event.pointerId) return;
    const deltaX = event.clientX - pointer.x;
    pointer.x = event.clientX;
    setCenterSense(current => Math.max(-1, Math.min(1, current + deltaX * CENTER_SENSE_DRAG_RATE)));
  };

  const releaseCenterSensePointer = (event: React.PointerEvent<HTMLButtonElement>) => {
    if (centerSensePointerRef.current?.pointerId !== event.pointerId) return;
    centerSensePointerRef.current = null;
    if (event.currentTarget.hasPointerCapture(event.pointerId)) {
      event.currentTarget.releasePointerCapture(event.pointerId);
    }
  };

  const resetCenterSense = () => setCenterSense(0);
  const resetCenterSenseFromContextMenu = (event: React.MouseEvent<HTMLButtonElement>) => {
    event.preventDefault();
    resetCenterSense();
  };
  const currentPlaybackItem = TRANSPORT_HISTORY_ITEMS[playbackHistoryIndex];
  const navigatePlaybackHistory = (previous: boolean) => {
    setPlaybackHistoryIndex(current => previous
      ? Math.max(0, current - 1)
      : Math.min(TRANSPORT_HISTORY_ITEMS.length - 1, current + 1));
    setPlaybackProgress(0);
    setPlaying(true);
  };

  return <footer className="runtime-region runtime-transport" style={style} data-source="src/main.cpp: transport and waveform panels">
    <span className="top-grip">•••</span>
    <div className="transport-bar" data-source="src/main.cpp:50060-51970">
      <strong className="transport-timecode">{formatTransportTime(playbackProgress)}</strong>
      <div
        className="transport-volume"
        role="slider"
        aria-label="Volume"
        aria-valuemin={0}
        aria-valuemax={120}
        aria-valuenow={Math.round(volumePercent)}
        data-source="src/main.cpp:50770-50966"
        onPointerDown={handleVolumePointerDown}
        onPointerMove={handleVolumePointerMove}
        onPointerUp={releaseVolumePointer}
        onPointerCancel={releaseVolumePointer}
        onWheel={handleVolumeWheel}
        onContextMenu={resetVolumeFromContextMenu}>
        <span className="volume-signal" style={{ width: '96.67%' }} />
        <span className="volume-normal" style={{ width: `${Math.min(volumePercent, 100) / 1.2}%` }} />
        <span className="volume-overdrive" style={{ width: `${Math.max(0, volumePercent - 100) / 1.2}%` }} />
        <i>{Math.round(volumePercent)}%</i>
        <b />
      </div>
      <button type="button" className={`transport-stereo ${mono ? 'active' : ''}`} aria-label="Mono stereo" onClick={() => setMono(value => !value)}>
        <TransportStereoGlyph mono={mono} offset={stereoOffset} scale={stereoScale} />
      </button>
      <button type="button" className={`transport-reverse ${reversed ? 'active' : ''}`} aria-label="Reverse" onClick={() => setReversed(value => !value)}>
        <TransportReverseGlyph progress={reverseProgress} />
      </button>
      <button type="button" className={`transport-norm ${normalized ? 'active' : ''}`} onClick={() => setNormalized(value => !value)}>NORM</button>
      <button
        type="button"
        className="transport-pan"
        aria-label="Pan"
        aria-valuemin={-50}
        aria-valuemax={50}
        aria-valuenow={Math.round(panValue)}
        disabled={mono}
        onPointerDown={handlePanPointerDown}
        onPointerMove={handlePanPointerMove}
        onPointerUp={releasePanPointer}
        onPointerCancel={releasePanPointer}
        onDoubleClick={resetPan}
        onContextMenu={resetPanFromContextMenu}>
        <TransportPanGlyph value={panValue} />
      </button>
      <button
        type="button"
        className="transport-center-sense"
        aria-label="Center sense"
        aria-valuemin={-1}
        aria-valuemax={1}
        aria-valuenow={centerSense}
        disabled={mono}
        onPointerDown={handleCenterSensePointerDown}
        onPointerMove={handleCenterSensePointerMove}
        onPointerUp={releaseCenterSensePointer}
        onPointerCancel={releaseCenterSensePointer}
        onDoubleClick={resetCenterSense}
        onContextMenu={resetCenterSenseFromContextMenu}>
        <TransportCenterSenseGlyph value={centerSense} />
      </button>
      <TransportLoopButton active={looping} onClick={() => setLooping(value => !value)} />
      <div className="transport-right">
        <b>11782 files</b>
        <TransportDbSwitch value={dbName} options={TRANSPORT_DB_OPTIONS} onChange={setDbName} />
        <i className="transport-divider" />
        <label className="transport-spectrum"><input type="checkbox" /> <span>Spectrum</span></label>
      </div>
    </div>
    <div className="wave-area" data-source="src/main.cpp:28549 DrawMainWaveform">
      <div className="wave-amplitude" style={{ height: `${Math.min(80, Math.max(40, 171 * 0.45))}px` }} role="slider" aria-label="Waveform amplitude" aria-valuemin={WAVE_AMPLITUDE_MIN} aria-valuemax={WAVE_AMPLITUDE_MAX} aria-valuenow={waveformAmplitude} onPointerDown={handleWaveAmplitudePointerDown} onPointerMove={handleWaveAmplitudePointerMove} onPointerUp={releaseWaveAmplitudePointer} onPointerCancel={releaseWaveAmplitudePointer} onWheel={handleWaveAmplitudeWheel} onContextMenu={resetWaveAmplitude} data-source="src/main.cpp:53492-53551"><span className="wave-amplitude-track" /><i className="wave-amplitude-knob" style={{ bottom: `${((waveformAmplitude - WAVE_AMPLITUDE_MIN) / (WAVE_AMPLITUDE_MAX - WAVE_AMPLITUDE_MIN)) * 60 + 16}px` }} /><b>+</b></div>
      <div className="timeline">{times.map(time => <span key={time}>{time}</span>)}</div>
      <svg className="main-wave top" style={{ transform: `scaleY(${waveformAmplitude})` }} viewBox={`${waveformViewCenter * (2380 - 2380 / waveformZoom)} 0 ${2380 / waveformZoom} 120`} preserveAspectRatio="none"><path d={mainWave} /></svg>
      <svg className="main-wave bottom" style={{ transform: `scaleY(-${waveformAmplitude})` }} viewBox={`${waveformViewCenter * (2380 - 2380 / waveformZoom)} 0 ${2380 / waveformZoom} 120`} preserveAspectRatio="none"><path d={mainWave} /></svg>
      <div className="wave-interaction" onPointerDown={handleWaveformSeek} onWheel={handleWaveformWheel} data-source="src/main.cpp:52719-52728"><div className="playhead" style={{ left: `${playbackProgress * 100}%` }} /></div>
      <div className="wave-scroll" onPointerDown={handleWaveScrollPointerDown} onPointerMove={handleWaveScrollPointerMove} onPointerUp={releaseWaveScrollPointer} onPointerCancel={releaseWaveScrollPointer} data-source="src/main.cpp:52789-52930"><span style={{ width: `${100 / waveformZoom}%`, left: `${waveformViewCenter * (100 - 100 / waveformZoom)}%` }} /></div>
      <div className="meter"><em>-1</em><em>-10</em><i /><i /></div>
    </div>
    <div className="status-row" data-source="src/main.cpp:53155-53242 NavigatePlaybackHistory">
      <span>{playing ? `播放中: ${currentPlaybackItem}` : `当前: ${currentPlaybackItem}`}</span>
      <div className="play-controls">
        <button type="button" aria-label="Previous" disabled={playbackHistoryIndex === 0} onClick={() => navigatePlaybackHistory(true)}><TransportGlyph name="prev" /></button>
        <button type="button" onClick={() => setPlaying(value => !value)} aria-label={playing ? 'Pause' : 'Play'}><TransportGlyph name={playing ? 'pause' : 'play'} /></button>
        <button type="button" aria-label="Next" disabled={playbackHistoryIndex === TRANSPORT_HISTORY_ITEMS.length - 1} onClick={() => navigatePlaybackHistory(false)}><TransportGlyph name="next" /></button>
        <button type="button" className={shuffle ? 'active' : ''} onClick={() => setShuffle(value => !value)} aria-label="Shuffle"><TransportShuffleGlyph progress={footerShuffleProgress} /></button>
        <button type="button" className={listeningMode !== 'browse' ? 'active' : ''} onClick={() => setListeningMode(mode => mode === 'browse' ? 'audition' : 'browse')} aria-label="Listening mode"><TransportListeningGlyph mode={listeningMode} /></button>
      </div>
      <TransportDawWidget />
    </div>
  </footer>;
}

import React, { useEffect, useRef, useState } from 'react';

const PRESET_ITEM_SPACING = 58;
const PRESET_WHEEL_STEP = 0.5;
const PRESET_SNAP_RATE = 10;
const PRESET_SETTLE_EPSILON = 0.08;
const PRESET_DRAG_MIN = -1;
const INITIAL_PRESETS = ['SFX', 'Music', 'Meta', 'Loud'];

const clamp = (value: number, min: number, max: number) => Math.max(min, Math.min(max, value));

export default function RuntimePresetDial() {
  const [presets, setPresets] = useState(() => INITIAL_PRESETS.map(name => ({ name, builtIn: true })));
  const [activeIndex, setActiveIndex] = useState(0);
  const [scrollPosition, setScrollPosition] = useState(0);
  const [isDragging, setIsDragging] = useState(false);
  const [showAddPreset, setShowAddPreset] = useState(false);
  const [newPresetName, setNewPresetName] = useState('');
  const [showContextMenu, setShowContextMenu] = useState(false);
  const scrollRef = useRef(0);
  const animationRef = useRef<number | null>(null);
  const dragRef = useRef({ pointerId: -1, startX: 0, startScroll: 0 });

  const updateScroll = (value: number) => {
    scrollRef.current = value;
    setScrollPosition(value);
  };

  const settleToNearestPreset = () => {
    if (animationRef.current !== null) cancelAnimationFrame(animationRef.current);
    let previous = performance.now();
    const animate = (now: number) => {
      const dt = Math.min((now - previous) / 1000, 0.05);
      previous = now;
      const target = clamp(Math.round(scrollRef.current), PRESET_DRAG_MIN, presets.length - 1);
      const next = scrollRef.current + (target - scrollRef.current) * Math.min(1, PRESET_SNAP_RATE * dt);
      updateScroll(next);
      if (Math.abs(next - target) < PRESET_SETTLE_EPSILON) {
        updateScroll(target);
        if (target >= 0) setActiveIndex(target);
        animationRef.current = null;
        return;
      }
      animationRef.current = requestAnimationFrame(animate);
    };
    animationRef.current = requestAnimationFrame(animate);
  };

  useEffect(() => () => {
    if (animationRef.current !== null) cancelAnimationFrame(animationRef.current);
  }, []);

  const handleWheel = (event: React.WheelEvent<HTMLDivElement>) => {
    event.preventDefault();
    if (event.deltaY === 0) return;
    const next = clamp(scrollRef.current + Math.sign(event.deltaY) * PRESET_WHEEL_STEP, PRESET_DRAG_MIN, presets.length - 1);
    updateScroll(next);
    settleToNearestPreset();
  };

  const startDrag = (event: React.PointerEvent<HTMLDivElement>) => {
    if (event.button !== 0) return;
    event.currentTarget.setPointerCapture(event.pointerId);
    dragRef.current = { pointerId: event.pointerId, startX: event.clientX, startScroll: scrollRef.current };
    setIsDragging(true);
    if (animationRef.current !== null) cancelAnimationFrame(animationRef.current);
  };

  const moveDrag = (event: React.PointerEvent<HTMLDivElement>) => {
    if (!isDragging || dragRef.current.pointerId !== event.pointerId) return;
    const next = dragRef.current.startScroll - (event.clientX - dragRef.current.startX) / PRESET_ITEM_SPACING;
    updateScroll(clamp(next, PRESET_DRAG_MIN, presets.length));
  };

  const finishDrag = (event: React.PointerEvent<HTMLDivElement>) => {
    if (dragRef.current.pointerId !== event.pointerId) return;
    event.currentTarget.releasePointerCapture(event.pointerId);
    setIsDragging(false);
    if (Math.abs(scrollRef.current - PRESET_DRAG_MIN) < 0.25) {
      updateScroll(PRESET_DRAG_MIN);
      setShowAddPreset(true);
      return;
    }
    settleToNearestPreset();
  };

  const addPreset = () => {
    const name = newPresetName.trim();
    if (!name) return;
    const nextPresets = [...presets, { name, builtIn: false }];
    setPresets(nextPresets);
    setActiveIndex(nextPresets.length - 1);
    updateScroll(nextPresets.length - 1);
    setNewPresetName('');
    setShowAddPreset(false);
  };

  const openContextMenu = (event: React.MouseEvent<HTMLDivElement>) => {
    event.preventDefault();
    setShowContextMenu(true);
  };

  const deleteActivePreset = () => {
    if (presets[activeIndex]?.builtIn) return;
    const nextPresets = presets.filter((_, index) => index !== activeIndex);
    const nextIndex = clamp(activeIndex, 0, nextPresets.length - 1);
    setPresets(nextPresets);
    setActiveIndex(nextIndex);
    updateScroll(nextIndex);
    setShowContextMenu(false);
  };

  const virtualItems = [{ name: '+', builtIn: true }, ...presets];
  return <>
    <div className="preset-dial" role="listbox" aria-label="Column preset" tabIndex={0}
      onWheel={handleWheel} onPointerDown={startDrag} onPointerMove={moveDrag}
      onPointerUp={finishDrag} onPointerCancel={finishDrag} onContextMenu={openContextMenu}
      onKeyDown={event => {
        if (event.key !== 'ArrowLeft' && event.key !== 'ArrowRight') return;
        event.preventDefault();
        updateScroll(clamp(activeIndex + (event.key === 'ArrowRight' ? 1 : -1), 0, presets.length - 1));
        settleToNearestPreset();
      }}>
      <i className="preset-center-tick top" /><i className="preset-center-tick bottom" />
      {virtualItems.map((preset, itemIndex) => {
        const virtualIndex = itemIndex - 1;
        const distance = Math.abs(virtualIndex - scrollPosition);
        if (distance > 2.5) return null;
        const x = 80 + (virtualIndex - scrollPosition) * PRESET_ITEM_SPACING;
        const edgeLeft = clamp((x - 10) / 10, 0, 1);
        const edgeRight = clamp((150 - x) / 10, 0, 1);
        const opacity = virtualIndex === activeIndex && distance < 0.12
          ? 1
          : clamp(1 - distance * 0.42, 0, 1) * edgeLeft * edgeRight;
        return <span role="option" aria-selected={virtualIndex === activeIndex} key={`${preset.name}-${itemIndex}`}
          className={`preset-item ${virtualIndex === activeIndex && distance < 0.12 ? 'active' : ''} ${virtualIndex === -1 ? 'add' : ''}`}
          style={{ left: x, opacity, transform: `translate(-50%, ${virtualIndex < 0 ? 0 : distance * 1.5}px)` }}>{preset.name}</span>;
      })}
      <i className="preset-edge-fade left" /><i className="preset-edge-fade right" />
    </div>
    {showAddPreset && <form className="preset-add-popover" onSubmit={event => { event.preventDefault(); addPreset(); }}>
      <input autoFocus maxLength={32} aria-label="New preset name" value={newPresetName} onChange={event => setNewPresetName(event.target.value)} />
      <button type="submit">Save</button>
    </form>}
    {showContextMenu && <div className="preset-context-menu" role="menu">
      <button role="menuitem" disabled={presets[activeIndex]?.builtIn} onClick={deleteActivePreset}>删除 “{presets[activeIndex]?.name}”</button>
      {presets[activeIndex]?.builtIn && <small>Built-in preset</small>}
    </div>}
  </>;
}

import React, { useEffect, useLayoutEffect, useRef, useState } from 'react';
import { createPortal } from 'react-dom';

export interface DropdownMenuItem {
  id: string;
  label: string;
  disabled?: boolean;
  checked?: boolean;
  shortcut?: string;
  separator?: boolean;
  sectionLabel?: string;
  children?: DropdownMenuItem[];
  onClick?: () => void;
}

interface DropdownMenuProps {
  open: boolean;
  items: DropdownMenuItem[];
  anchorEl?: HTMLElement | null;
  onHoverChange?: (hovered: boolean) => void;
  onClose: () => void;
}

function SubMenu({ items, onClose }: { items: DropdownMenuItem[]; onClose: () => void }) {
  return (
    <div className="dropdown-submenu">
      {items.map((item) => (
        <MenuItemRow key={item.id} item={item} onClose={onClose} />
      ))}
    </div>
  );
}

function MenuItemRow({ item, onClose }: { item: DropdownMenuItem; onClose: () => void }) {
  const [subOpen, setSubOpen] = useState(false);
  const timerRef = useRef<number>(0);

  if (item.separator) {
    return <hr className="dropdown-separator" />;
  }
  if (item.sectionLabel) {
    return <div className="dropdown-section-label">{item.sectionLabel}</div>;
  }

  const hasChildren = item.children && item.children.length > 0;

  return (
    <div
      className="dropdown-item-wrap"
      onPointerEnter={() => {
        if (hasChildren) {
          clearTimeout(timerRef.current);
          setSubOpen(true);
        }
      }}
      onPointerLeave={() => {
        if (hasChildren) {
          timerRef.current = window.setTimeout(() => setSubOpen(false), 180);
        }
      }}
    >
      <button
        className={`dropdown-menu-item${item.checked ? ' dropdown-checked' : ''}`}
        disabled={item.disabled}
        onClick={() => {
          if (item.disabled) return;
          if (hasChildren) {
            setSubOpen((v) => !v);
            return;
          }
          item.onClick?.();
          onClose();
        }}
      >
        <span className="dropdown-check-col">{item.checked ? '✓' : ''}</span>
        <span className="dropdown-label-col">{item.label}</span>
        {item.shortcut && <span className="dropdown-shortcut">{item.shortcut}</span>}
        {hasChildren && <span className="dropdown-arrow-col">▸</span>}
      </button>
      {hasChildren && subOpen && <SubMenu items={item.children!} onClose={onClose} />}
    </div>
  );
}

export default function DropdownMenu({ open, items, anchorEl, onHoverChange, onClose }: DropdownMenuProps) {
  const menuRef = useRef<HTMLDivElement | null>(null);
  const [pos, setPos] = useState<{ left: number; top: number } | null>(null);

  useLayoutEffect(() => {
    if (!open) return;
    const update = () => {
      const r = anchorEl?.getBoundingClientRect?.();
      if (!r) {
        setPos({ left: 12, top: 40 });
        return;
      }
      const margin = 6;
      let left = Math.round(r.left);
      let top = Math.round(r.bottom + 4);
      // Clamp within viewport (avoid going off-screen on narrow widths)
      const vw = window.innerWidth || 1920;
      const estW = 220; // menu has min-width; final width may grow but this keeps it reasonable
      if (left + estW + margin > vw) left = Math.max(margin, vw - estW - margin);
      if (top < margin) top = margin;
      setPos({ left, top });
    };
    update();
    window.addEventListener('resize', update);
    return () => window.removeEventListener('resize', update);
  }, [open, anchorEl]);

  useEffect(() => {
    if (!open) return;
    const onPointerDown = (e: PointerEvent) => {
      const t = e.target as Node;
      if (!menuRef.current?.contains(t) && !(anchorEl && anchorEl.contains(t))) onClose();
    };
    window.addEventListener('pointerdown', onPointerDown, true);
    return () => window.removeEventListener('pointerdown', onPointerDown, true);
  }, [open, onClose, anchorEl]);

  if (!open) return null;

  return createPortal(
    <div
      className="dropdown-menu dropdown-menu-portal"
      ref={menuRef}
      onPointerEnter={() => onHoverChange?.(true)}
      onPointerLeave={() => onHoverChange?.(false)}
      style={{
        position: 'fixed',
        left: `${pos?.left ?? 12}px`,
        top: `${pos?.top ?? 40}px`,
      }}
    >
      {items.map((item) => (
        <MenuItemRow key={item.id} item={item} onClose={onClose} />
      ))}
    </div>,
    document.body,
  );
}

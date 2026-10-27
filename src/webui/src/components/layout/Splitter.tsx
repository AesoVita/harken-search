import React from 'react';

interface SplitterProps {
  orientation: 'vertical' | 'horizontal';
  onPointerDown: (e: React.PointerEvent<HTMLDivElement>) => void;
}

export default function Splitter({ orientation, onPointerDown }: SplitterProps) {
  return (
    <div
      className={`panel-splitter panel-splitter-${orientation}`}
      onPointerDown={onPointerDown}
      role="separator"
      aria-orientation={orientation}
      tabIndex={0}
    />
  );
}

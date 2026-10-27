import React from 'react';

interface ResizablePanelProps {
  title: string;
  collapsed?: boolean;
  onToggleCollapsed?: () => void;
  children: React.ReactNode;
  className?: string;
}

export default function ResizablePanel({
  title,
  collapsed = false,
  onToggleCollapsed,
  children,
  className,
}: ResizablePanelProps) {
  return (
    <section className={`resizable-panel ${className ?? ''}`}>
      <header className="panel-header">
        <span className="panel-title">{title}</span>
        {onToggleCollapsed && (
          <button className="panel-toggle-btn" onClick={onToggleCollapsed}>
            {collapsed ? '展开' : '收起'}
          </button>
        )}
      </header>
      {!collapsed && <div className="panel-body">{children}</div>}
    </section>
  );
}

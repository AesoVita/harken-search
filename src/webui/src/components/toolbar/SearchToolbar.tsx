import React, { useMemo, useState } from 'react';
import type { SearchFilters } from '../../types';

interface SearchToolbarProps {
  query: string;
  setQuery: (v: string) => void;
  loading: boolean;
  suggestions: string[];
  onPickSuggestion: (value: string) => void;
  filters: SearchFilters;
  setFilters: (value: SearchFilters) => void;
  onSaveSearch: () => void;
  onRandom: () => void;
  onBack: () => void;
  onForward: () => void;
  totalCount: number;
}

const FILE_TYPES = ['showWAV', 'showMP3', 'showFLAC', 'showOGG', 'showAIFF'] as const;
type FileTypeKey = (typeof FILE_TYPES)[number];

export default function SearchToolbar(props: SearchToolbarProps) {
  const {
    query,
    setQuery,
    loading,
    suggestions,
    onPickSuggestion,
    filters,
    setFilters,
    onSaveSearch,
    onRandom,
    onBack,
    onForward,
    totalCount,
  } = props;

  const [suggestOpen, setSuggestOpen] = useState(false);

  const selectedFilterLabel = useMemo(() => {
    return FILE_TYPES.filter((k) => filters[k]).map((k) => k.replace('show', '')).join('/');
  }, [filters]);

  return (
    <div className="search-toolbar-glass">
      <div className="search-toolbar-left">
        <div className="history-nav-glass" role="group" aria-label="播放历史导航">
          <button
            className="history-nav-btn history-nav-btn-back"
            onClick={onBack}
            title="上一页（播放历史后退）"
            aria-label="上一页"
          >
            <span className="history-nav-icon" aria-hidden="true">←</span>
            <span className="history-nav-label">上一页</span>
          </button>
          <span className="history-nav-divider" aria-hidden="true" />
          <button
            className="history-nav-btn history-nav-btn-forward"
            onClick={onForward}
            title="下一页（播放历史前进）"
            aria-label="下一页"
          >
            <span className="history-nav-label">下一页</span>
            <span className="history-nav-icon" aria-hidden="true">→</span>
          </button>
        </div>
      </div>

      <div className="search-input-wrap">
        <input
          className="glass-input search-input-main"
          value={query}
          placeholder="搜索音效（200ms 防抖）..."
          onFocus={() => setSuggestOpen(true)}
          onBlur={() => setTimeout(() => setSuggestOpen(false), 120)}
          onChange={(e) => setQuery(e.target.value)}
        />
        {loading && <span className="search-spinner" />}
        {suggestOpen && suggestions.length > 0 && (
          <div className="suggest-dropdown">
            {suggestions.map((item) => (
              <button
                key={item}
                className="suggest-item-btn"
                onClick={() => {
                  onPickSuggestion(item);
                  setSuggestOpen(false);
                }}
              >
                {item}
              </button>
            ))}
          </div>
        )}
      </div>

      <div className="search-toolbar-actions">
        <button className="btn-secondary btn-compact" onClick={onSaveSearch}>
          保存搜索
        </button>

        <div className="file-type-filter-wrap">
          <span className="file-type-filter-title">类型</span>
          {FILE_TYPES.map((key: FileTypeKey) => (
            <label className="file-type-chip" key={key}>
              <input
                type="checkbox"
                checked={!!filters[key]}
                onChange={(e) => setFilters({ ...filters, [key]: e.target.checked })}
              />
              {key.replace('show', '')}
            </label>
          ))}
        </div>

        <button className="btn-icon btn-icon-sm" onClick={onRandom} title="随机骰子">
          🎲
        </button>

        <span className="total-count-badge">共 {totalCount} 条</span>
        <span className="type-summary">{selectedFilterLabel}</span>
      </div>
    </div>
  );
}

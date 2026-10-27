import React, { useMemo, useState } from 'react';
import type { FilterPanelProps, FilterState, FilterValue } from './types';

const GROUP_TITLES: Array<{ key: keyof FilterState; label: string }> = [
  { key: 'artist', label: 'Artist' },
  { key: 'album', label: 'Album' },
  { key: 'category', label: 'Category' },
  { key: 'tags', label: 'Tags' },
  { key: 'format', label: 'Format' },
  { key: 'sampleRate', label: 'Sample Rate' },
  { key: 'channels', label: 'Channels' },
  { key: 'duration', label: 'Duration' },
];

function toggleListValue<T extends string | number>(arr: T[], v: T) {
  return arr.includes(v) ? arr.filter((x) => x !== v) : [...arr, v];
}

export default function MetadataFilters(props: FilterPanelProps) {
  const { value, options, defaultValue, onChange, onReset, onClearAll, onRemoveChip, showChips = true } = props;
  const [expanded, setExpanded] = useState<Record<string, boolean>>({
    artist: false,
    album: false,
    category: false,
    tags: false,
    format: false,
    sampleRate: false,
    channels: false,
    duration: false,
  });

  const chips = useMemo(() => {
    const result: Array<{ key: keyof FilterState; value: FilterValue; text: string }> = [];
    GROUP_TITLES.forEach(({ key, label }) => {
      const current = value[key];
      if (Array.isArray(current)) {
        current.forEach((v) => result.push({ key, value: v, text: `${label}: ${v}` }));
      } else if (current && typeof current === 'object' && 'min' in current) {
        const min = current.min ?? '-';
        const max = current.max ?? '-';
        result.push({ key, value: `${min}-${max}`, text: `${label}: ${min}~${max}` });
      } else if (typeof current === 'string' && current !== 'any') {
        result.push({ key, value: current, text: `${label}: ${current}` });
      }
    });
    return result;
  }, [value]);

  return (
    <div className="mf-root">
      <div className="mf-header">
        <span className="mf-title">Metadata Filters</span>
        <button
          className="mf-reset"
          onClick={() => {
            onChange(defaultValue);
            onReset?.();
          }}
        >
          Reset
        </button>
      </div>

      <div className="mf-sections">
        <div className="mf-section">
          <button className="mf-section-head" onClick={() => setExpanded((p) => ({ ...p, artist: !p.artist }))}>
            <span>Artist</span><span>{expanded.artist ? '▾' : '▸'}</span>
          </button>
          {expanded.artist && options.artist.map((o) => {
            const active = value.artist.includes(o.value);
            return (
              <button
                key={o.value}
                className={`mf-option ${active ? 'mf-option-on' : ''}`}
                onClick={() => onChange({ ...value, artist: toggleListValue(value.artist, o.value) })}
              >
                <span>{o.label}</span><span>{o.count ?? 0}</span>
              </button>
            );
          })}
        </div>

        <div className="mf-section">
          <button className="mf-section-head" onClick={() => setExpanded((p) => ({ ...p, category: !p.category }))}>
            <span>Category</span><span>{expanded.category ? '▾' : '▸'}</span>
          </button>
          {expanded.category && options.category.map((o) => {
            const active = value.category.includes(o.value);
            return (
              <button
                key={o.value}
                className={`mf-option ${active ? 'mf-option-on' : ''}`}
                onClick={() => onChange({ ...value, category: toggleListValue(value.category, o.value) })}
              >
                <span>{o.label}</span><span>{o.count ?? 0}</span>
              </button>
            );
          })}
        </div>

        <div className="mf-section">
          <button className="mf-section-head" onClick={() => setExpanded((p) => ({ ...p, tags: !p.tags }))}>
            <span>Tags</span><span>{expanded.tags ? '▾' : '▸'}</span>
          </button>
          {expanded.tags && options.tags.map((o) => {
            const active = value.tags.includes(o.value);
            return (
              <button
                key={o.value}
                className={`mf-option ${active ? 'mf-option-on' : ''}`}
                onClick={() => onChange({ ...value, tags: toggleListValue(value.tags, o.value) })}
              >
                <span>{o.label}</span><span>{o.count ?? 0}</span>
              </button>
            );
          })}
        </div>

        <div className="mf-section">
          <button className="mf-section-head" onClick={() => setExpanded((p) => ({ ...p, format: !p.format }))}>
            <span>Format</span><span>{expanded.format ? '▾' : '▸'}</span>
          </button>
          {expanded.format && options.format.map((o) => {
            const active = value.format.includes(o.value);
            return (
              <button
                key={o.value}
                className={`mf-option ${active ? 'mf-option-on' : ''}`}
                onClick={() => onChange({ ...value, format: toggleListValue(value.format, o.value) })}
              >
                <span>{o.label}</span><span>{o.count ?? 0}</span>
              </button>
            );
          })}
        </div>
      </div>

      {showChips && (
        <div className="mf-chips">
          {chips.map((chip) => (
            <span key={`${chip.key}-${chip.value}`} className="mf-chip">
              {chip.text}
              <button
                onClick={() => {
                  onRemoveChip?.(chip.key, chip.value);
                  if (chip.key === 'artist') onChange({ ...value, artist: value.artist.filter((v) => v !== chip.value) });
                  if (chip.key === 'category') onChange({ ...value, category: value.category.filter((v) => v !== chip.value) });
                  if (chip.key === 'tags') onChange({ ...value, tags: value.tags.filter((v) => v !== chip.value) });
                  if (chip.key === 'format') onChange({ ...value, format: value.format.filter((v) => v !== chip.value) });
                }}
              >
                ×
              </button>
            </span>
          ))}
          {!!chips.length && (
            <button
              className="mf-clear"
              onClick={() => {
                onChange(defaultValue);
                onClearAll?.();
              }}
            >
              Clear All
            </button>
          )}
        </div>
      )}
    </div>
  );
}

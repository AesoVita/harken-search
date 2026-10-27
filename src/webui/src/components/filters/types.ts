export type FilterValue = string | number | boolean;

export interface RangeValue {
  min?: number;
  max?: number;
}

export interface FilterState {
  artist: string[];
  album: string[];
  category: string[];
  tags: string[];
  format: string[];
  sampleRate: number[];
  channels?: 'any' | 'mono' | 'stereo';
  duration?: RangeValue;
}

export interface FilterOptionItem {
  value: string;
  label: string;
  count?: number;
}

export interface FilterOptions {
  artist: FilterOptionItem[];
  album: FilterOptionItem[];
  category: FilterOptionItem[];
  tags: FilterOptionItem[];
  format: FilterOptionItem[];
  sampleRate: { value: number; label: string; count?: number }[];
  channels: { value: 'any' | 'mono' | 'stereo'; label: string }[];
}

export interface FilterPanelProps {
  value: FilterState;
  options: FilterOptions;
  defaultValue: FilterState;
  onChange: (next: FilterState) => void;
  onApply?: (next: FilterState) => void;
  onReset?: () => void;
  onClearAll?: () => void;
  onRemoveChip?: (key: keyof FilterState, v: FilterValue) => void;
  showChips?: boolean;
}

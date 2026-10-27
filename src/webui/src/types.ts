export interface AudioRecord {
  id: number;
  fileName: string;
  filePath: string;
  folderPath: string;
  duration: number;
  durationSeconds?: number;
  format: string;
  extension?: string;
  sampleRate: number;
  channels: number;
  bitDepth: number;
  fileSize: number;
  description: string;
  keywords: string;
  category: string;
  artist: string;
  album: string;
  loudnessLUFS: number;
  rmsDb: number;
  truePeakDbTP: number;
  hasCover: boolean;
  /** coverKey for cached thumbnail (embeddedCoverKey or folderCoverKey). Empty if unknown. */
  coverKey?: string;
  /** cover cache revision marker, changes when cover changes. */
  coverRev?: string;
  dateAdded: string;
}

export interface PanelStateDto {
  visible: boolean;
  dockedWidth: number;
  dockedHeight: number;
}

export interface PanelStateResponse {
  panelSidebar: PanelStateDto;
  panelFileInfo: PanelStateDto;
  panelWaveform: PanelStateDto;
}

export interface AppSettingsDto {
  playbackMode: number;
  searchRealtime: boolean;
  autoplayOnSelect?: boolean;
  continuousPlayback?: boolean;
  shufflePlayback?: boolean;
  playerSelectionLink?: boolean;
  showSpectrogram?: boolean;
  spectrogramBlend?: number;
  spectrogramFFTSize?: number;
  spectrogramSplit?: boolean;
  showMilliseconds?: boolean;
  showFrameNumber?: boolean;
  defaultFrameRate?: number;
  rowHeight: number;
  fontSize: number;
  waveformPanelHeight: number;
  showFileExtension: boolean;
  useWebUIMain: boolean;
  webUiLayout?: {
    panel?: { left?: number; right?: number; bottom?: number };
    column?: (Record<string, number> & { order?: string[]; hidden?: string[] });
    rowHeight?: number;
  };
}

export interface SearchFilters {
  showWAV: boolean;
  showMP3: boolean;
  showFLAC: boolean;
  showOGG: boolean;
  showAIFF: boolean;
  // Cover filters (match ImGui): 0=Any, 1=Has cover, 2=No cover
  coverFilter?: 0 | 1 | 2;
  // Same cover key filter: "E|..." or "F|..."
  specificCoverKey?: string;
}

export interface FileCoverResponse {
  hasCover: boolean;
  source: 'embedded' | 'folder' | 'placeholder' | 'too_large';
  dataUrl: string; // data:image/...;base64,...
  fileUrl: string; // raw file path for folder cover (informational)
  cacheKey: string; // CoverKey used by DB/CoverModule, e.g. "E|..." or "F|..."
  mime: string;
}

export interface SearchQueryResponse {
  items: AudioRecord[];
  total: number;
  offset: number;
  limit: number;
}

export interface FolderNode {
  path: string;
  name: string;
  fileCount: number;
  subfolders: FolderNode[];
}

export interface PlaylistItem {
  id: number;
  name: string;
}

export interface FileInfoDetails {
  id: number;
  fileName: string;
  filePath: string;
  folderPath: string;
  duration: number;
  format: string;
  sampleRate: number;
  channels: number;
  bitDepth: number;
  fileSize: number;
  description: string;
  keywords: string;
  category: string;
  artist: string;
  album: string;
  hasCover: boolean;
  dateAdded: string;
  fileMtime: string;
  lastIndexed: string;
  totalFrames: number;
}

export interface TransportState {
  playing: boolean;
  loaded: boolean;
  position: number; // normalized 0..1
  duration: number; // seconds
  pitch: number;
  volume: number;
  inPoint: number;
  outPoint: number;
  fileId?: number;
  reversed?: boolean;
}

export interface WaveformMiniResponse {
  peaks: number[];
  rms: number[];
}

export interface SpectrogramTileResponse {
  w: number;
  h: number;
  fftSize: number;
  startNorm: number;
  endNorm: number;
  rgba: string; // base64 raw RGBA bytes
}
export type AnnotationZone = {
  stableId: string;
  key: string;
  displayName: string;
  sourceAnchor: string;
  minX: number;
  minY: number;
  maxX: number;
  maxY: number;
  area: number;
  order: number;
};

export type AnnotationFrame = {
  width: number;
  height: number;
  sequence: number;
  ageSeconds: number | null;
  available: boolean;
};

export type AnnotationSession = {
  windowTitle: string;
  frame: AnnotationFrame;
  zoneFrameNumber: number | null;
  zoneExportAvailable: boolean;
  captureFailures: number;
  synchronized: boolean;
  error: string;
};

export type AnnotationZoneDocument = {
  schemaVersion?: number;
  frameNumber?: number;
  displayWidth: number;
  displayHeight: number;
  synchronized: boolean;
  zones: AnnotationZone[];
  error?: string;
};

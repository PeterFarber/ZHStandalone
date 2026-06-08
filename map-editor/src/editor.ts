/**
 * Map editor core: canvas tools, selection, undo history, and JSON export.
 * UI chrome is built in main.ts; this class owns world state and hit-testing.
 * Exported maps must stay aligned with game/include/zh/game/map_definition.hpp.
 */
import { createDefaultMap } from "./defaultMap";
import {
  backgroundImageUrl,
  defaultBackgroundDest,
  effectiveBackgroundDest,
} from "./background";
import {
  buildDrawableOrder,
  drawableEntryKey,
  effectiveSelectionZ,
  materializeDrawableZ,
  selectionHasZOrder,
  selectionToDrawableKey,
  setSelectionZValue,
  zCapableSelections,
} from "./drawOrder";
import { playAreaCenter, playAreaFromCenter } from "./boundsUtils";
import { applyFixedMapFrame } from "./mapFrame";
import {
  CAMERA_PREVIEW_LEVELS,
  worldViewRectAtFraction,
} from "./playingCamera";
import {
  arcFrom3Points,
  arcWithTangentAtEndpoint,
  boundsOfArc,
  drawArcPath,
  drawArcPoints,
  hitTestArc,
  lineDirection,
  paintArcPath,
  pointsNear,
  translateArc,
} from "./arcGeometry";
import {
  drawLineSegment,
  hitTestLine,
  lineBounds,
} from "./lineGeometry";
import { MapHistory } from "./history";
import {
  colliderDisplayTag,
  defaultColliderTag,
  defaultGoalTag,
  defaultShapeTag,
  defaultVisualTag,
  goalDisplayTag,
  objectTag,
  selectionSupportsTag,
  setObjectTag,
  shapeDisplayTag,
  spawnTagColor,
  visualDisplayTag,
} from "./objectTags";
import { isObjectLocked, selectionSupportsLock, setObjectLocked } from "./objectLocks";
import { listMapObjects, type MapObjectListEntry } from "./objectList";
import { normalizeMapFile } from "./normalizeMap";
import { eastGoalColor, westGoalColor } from "./teamColors";
import {
  clampCornerRadius,
  paintRoundRect,
  pointInRoundRect,
  roundnessHandleAt,
  traceRoundRectPath,
} from "./roundRectGeometry";
import {
  DEFAULT_TILE_CELL,
  type SheetKey,
  toolSheet,
  visualKindSheet,
} from "./tileSheets";
import type {
  Arc3,
  Collider,
  DrawShape,
  GoalZone,
  MapBackground,
  MapFile,
  MapShape,
  Point,
  Rect,
  Selection,
  ShapeStyle,
  SpawnKey,
  Tool,
  VisualPiece,
} from "./types";
import {
  allSpawnChecks,
  getSpawnPoint,
  isSpawnTool,
  rectFromPoints,
  rectsIntersect,
  selectionEquals,
  setSpawnPoint,
  spawnKeyLabel,
  spawnToolToKey,
} from "./types";
import { CLICK_SHAPES, CIRCLE_SRC, WALL_SLICES } from "./types";

type EditHandle =
  | { sel: Selection; kind: "point"; index: 0 | 1 | 2 }
  | { sel: Selection; kind: "center" }
  | { sel: Selection; kind: "radius" }
  | { sel: Selection; kind: "corner"; corner: 0 | 1 | 2 | 3 }
  | { sel: Selection; kind: "roundness" };

type Assets = Record<SheetKey, HTMLImageElement | null>;

export type MapSettingsPatch = {
  gridSize?: number;
  background?: MapBackground | null;
  playAreaWidth?: number;
  playAreaHeight?: number;
};

function uid(prefix: string): string {
  return `${prefix}_${Math.random().toString(36).slice(2, 9)}`;
}

function gridCellIndex(v: number, step: number): number {
  return Math.floor(v / step);
}

function gridCellCenter(col: number, row: number, step: number): Point {
  return { x: (col + 0.5) * step, y: (row + 0.5) * step };
}

/** Nine snap anchors per major grid cell: corners, edge midpoints, center. */
function gridCellAnchors(col: number, row: number, step: number): Point[] {
  const x0 = col * step;
  const y0 = row * step;
  const x1 = x0 + step;
  const y1 = y0 + step;
  const mx = x0 + step * 0.5;
  const my = y0 + step * 0.5;
  return [
    { x: x0, y: y0 },
    { x: x1, y: y0 },
    { x: x1, y: y1 },
    { x: x0, y: y1 },
    { x: mx, y: y0 },
    { x: x1, y: my },
    { x: mx, y: y1 },
    { x: x0, y: my },
    { x: mx, y: my },
  ];
}

/** Snap a scalar to the nearest major-cell anchor (corner, edge mid, or center along that axis). */
function snapScalarToGridAnchors(v: number, step: number): number {
  const half = step * 0.5;
  return Math.round(v / half) * half;
}

function snapPointToTile(p: Point, step: number): Point {
  const col = gridCellIndex(p.x, step);
  const row = gridCellIndex(p.y, step);
  let best = gridCellCenter(col, row, step);
  let bestD = dist(p.x, p.y, best.x, best.y);
  for (let dc = -1; dc <= 1; dc++) {
    for (let dr = -1; dr <= 1; dr++) {
      for (const anchor of gridCellAnchors(col + dc, row + dr, step)) {
        const d = dist(p.x, p.y, anchor.x, anchor.y);
        if (d < bestD) {
          bestD = d;
          best = anchor;
        }
      }
    }
  }
  return best;
}

function gridCellRect(col: number, row: number, step: number): Rect {
  return { x: col * step, y: row * step, w: step, h: step };
}

/** Axis-aligned rect from two pointer positions, both corners snapped to grid anchors. */
function rectFromSnapAnchors(p0: Point, p1: Point, step: number): Rect {
  const a = snapPointToTile(p0, step);
  const b = snapPointToTile(p1, step);
  const half = step * 0.5;
  const x0 = Math.min(a.x, b.x);
  const y0 = Math.min(a.y, b.y);
  const x1 = Math.max(a.x, b.x);
  const y1 = Math.max(a.y, b.y);
  return {
    x: x0,
    y: y0,
    w: Math.max(half, x1 - x0),
    h: Math.max(half, y1 - y0),
  };
}

function pointInRect(px: number, py: number, r: Rect): boolean {
  return px >= r.x && px <= r.x + r.w && py >= r.y && py <= r.y + r.h;
}

function dist(ax: number, ay: number, bx: number, by: number): number {
  return Math.hypot(ax - bx, ay - by);
}

function cloneRect(r: Rect): Rect {
  return { x: r.x, y: r.y, w: r.w, h: r.h };
}

export class MapEditor {
  map: MapFile = createDefaultMap();
  tool: Tool = "select";
  drawShape: DrawShape = "square";
  selections: Selection[] = [];
  gridSize = 8;
  showGrid = true;
  showLogic = true;
  useTileSrc = true;

  /** Active style for the Color draw tool (and default for new shapes). */
  drawFill = "rgba(77, 171, 255, 0.33)";
  drawStroke = "#ffffff";
  drawStrokeWidth = 2;
  drawFillEnabled = true;
  drawStrokeEnabled = true;
  /** Default corner radius for new square colliders / color rects (world units). */
  cornerRadius = 32;

  camX = 1200;
  camY = 900;
  zoom = 0.45;

  activeSrc: Partial<Record<SheetKey, Rect>> = {
    circle: cloneRect(CIRCLE_SRC),
  };
  tileCellSizes: Record<SheetKey, number> = { ...DEFAULT_TILE_CELL };

  onMapChange?: () => void;

  private assets: Assets = { ice: null, walls: null, circle: null, goal: null };
  private backgroundImage: HTMLImageElement | null = null;
  private backgroundImageKey: string | null = null;
  private backgroundBlobUrl: string | null = null;
  private history = new MapHistory();
  private dragMode: "pan" | "move" | "draw" | "marquee" | "point" | null = null;
  private dragStartWorld: Point = { x: 0, y: 0 };
  private dragStartSnapped: Point | null = null;
  private activeHandle: EditHandle | null = null;
  private marqueeStartWorld: Point = { x: 0, y: 0 };
  private marqueeRect: Rect | null = null;
  /** Right-click pick-through: cycle stacked hits at the same cursor spot. */
  private pickThroughPoint: Point | null = null;
  private pickThroughHits: Selection[] = [];
  private pickThroughIndex = 0;
  private dragMoveStates: Array<{
    sel: Selection;
    rect: Rect | null;
    arc: Arc3 | null;
    line: { p1: Point; p2: Point } | null;
  }> = [];
  private statusMessage = "";
  private clickPoints: Point[] = [];
  private previewRect: Rect | null = null;
  private previewCircle: { x: number; y: number; r: number } | null = null;
  private previewLine: { p1: Point; p2: Point } | null = null;
  private hoverCell: { col: number; row: number } | null = null;
  private hoverSnapped: Point | null = null;
  private pointerInside = false;
  private viewSize = { w: 1, h: 1 };

  constructor(
    private canvas: HTMLCanvasElement,
    private statusEl: HTMLElement,
  ) {
    this.loadAssets();
    this.bindCanvas();
    this.fitCamera();
    this.setStatus("Tool: select");
    this.loop();
  }

  setTool(tool: Tool) {
    this.tool = tool;
    this.clickPoints = [];
    this.clampDrawShape();
    this.updateToolStatus();
    this.onMapChange?.();
  }

  setDrawShape(shape: DrawShape) {
    this.drawShape = shape;
    this.clickPoints = [];
    this.previewRect = null;
    this.previewCircle = null;
    this.previewLine = null;
    this.updateToolStatus();
    this.onMapChange?.();
  }

  private updateToolStatus() {
    if (this.tool === "select") {
      this.setStatus("Select — drag to move (grid snap); drag green handles to edit points");
      return;
    }
    const shapeHint = CLICK_SHAPES.includes(this.drawShape)
      ? `${this.drawShape}: click ${this.clickPoints.length}/${this.drawShape === "arc" ? 3 : 2} points`
      : `${this.tool} · ${this.drawShape}: drag on grid`;
    this.setStatus(shapeHint);
  }

  private isDrawTool(tool: Tool = this.tool): boolean {
    return tool !== "select";
  }

  shapeSupported(shape: DrawShape): boolean {
    if (this.tool === "select") return false;
    if (this.tool === "color_shape" || this.tool === "collider") return true;
    if (this.tool === "goal_west" || this.tool === "goal_east") {
      return shape === "square" || shape === "circle" || shape === "arc";
    }
    return shape === "square" || shape === "circle";
  }

  private clampDrawShape() {
    if (!this.shapeSupported(this.drawShape)) {
      this.drawShape = "square";
    }
  }

  private effectiveShape(): DrawShape {
    return this.shapeSupported(this.drawShape) ? this.drawShape : "square";
  }

  private usesLogicOverlay(tool: Tool = this.tool): boolean {
    return (
      tool === "collider" ||
      tool === "goal_west" ||
      tool === "goal_east" ||
      isSpawnTool(tool)
    );
  }

  private shouldDrawLogic(): boolean {
    return this.showLogic || this.usesLogicOverlay();
  }

  private previewColor(): string {
    if (this.tool === "color_shape") {
      return this.drawStrokeEnabled ? this.drawStroke : this.drawFill;
    }
    if (this.tool === "collider") return "#ff9f43";
    if (this.tool === "goal_west") return westGoalColor();
    if (this.tool === "goal_east") return eastGoalColor();
    return "#ffd166";
  }

  private activeDrawStyle(): ShapeStyle {
    const style: ShapeStyle = {};
    if (this.drawFillEnabled && this.drawFill) style.fill = this.drawFill;
    if (this.drawStrokeEnabled && this.drawStroke) {
      style.stroke = this.drawStroke;
      style.strokeWidth = this.drawStrokeWidth;
    }
    return style;
  }

  private rectCornerRadiusForNew(rect: Rect): number | undefined {
    if (this.tool !== "collider" && this.tool !== "color_shape") return undefined;
    if (this.effectiveShape() !== "square") return undefined;
    const r = clampCornerRadius(rect, this.cornerRadius);
    return r > 0 ? r : undefined;
  }

  private selectionRectCornerRadius(sel: Selection): number {
    if (sel.type === "collider") {
      const c = this.map.colliders[sel.index];
      if (c?.kind === "rect") return c.cornerRadius ?? 0;
    }
    if (sel.type === "shape") {
      const s = this.map.shapes[sel.index];
      if (s?.kind === "rect") return s.cornerRadius ?? 0;
    }
    return 0;
  }

  private setSelectionRectCornerRadius(sel: Selection, radius: number) {
    const rect = this.selectionRect(sel);
    if (!rect) return;
    const r = clampCornerRadius(rect, radius);
    if (sel.type === "collider") {
      const c = this.map.colliders[sel.index];
      if (c?.kind === "rect") {
        if (r > 0) c.cornerRadius = r;
        else delete c.cornerRadius;
      }
    } else if (sel.type === "shape") {
      const s = this.map.shapes[sel.index];
      if (s?.kind === "rect") {
        if (r > 0) s.cornerRadius = r;
        else delete s.cornerRadius;
      }
    }
  }

  private clampStoredRectCornerRadius(sel: Selection) {
    const rect = this.selectionRect(sel);
    if (!rect) return;
    const r = clampCornerRadius(rect, this.selectionRectCornerRadius(sel));
    this.setSelectionRectCornerRadius(sel, r);
  }

  private supportsRoundRect(sel: Selection): boolean {
    if (sel.type === "collider") {
      return this.map.colliders[sel.index]?.kind === "rect";
    }
    if (sel.type === "shape") {
      return this.map.shapes[sel.index]?.kind === "rect";
    }
    return false;
  }

  setMap(map: MapFile, recordHistory = false) {
    this.map = applyFixedMapFrame(map);
    this.selections = [];
    this.fitCamera();
    void this.reloadBackgroundImage();
    if (recordHistory) {
      this.history.record(this.map);
    }
    this.onMapChange?.();
  }

  resetDefault() {
    this.history.record(this.map);
    this.setMap(createDefaultMap());
    this.setStatus("New empty map");
  }

  exportJson(): string {
    return JSON.stringify(applyFixedMapFrame(this.map), null, 2);
  }

  importJson(text: string) {
    this.history.record(this.map);
    this.setMap(normalizeMapFile(JSON.parse(text)));
    this.setStatus("Imported JSON");
  }

  download(filename = "default.json") {
    const blob = new Blob([this.exportJson()], { type: "application/json" });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = filename;
    a.click();
    URL.revokeObjectURL(url);
    this.setStatus(`Downloaded ${filename}`);
  }

  undo() {
    const prev = this.history.undo(this.map);
    if (!prev) {
      this.setStatus("Nothing to undo");
      return;
    }
    this.map = prev;
    this.selections = [];
    this.setStatus("Undo");
    this.onMapChange?.();
  }

  redo() {
    const next = this.history.redo(this.map);
    if (!next) {
      this.setStatus("Nothing to redo");
      return;
    }
    this.map = next;
    this.selections = [];
    this.setStatus("Redo");
    this.onMapChange?.();
  }

  getActiveSheet(): SheetKey | null {
    const visualSel = this.selections.find((s) => s.type === "visual");
    if (visualSel?.type === "visual") {
      const piece = this.map.visuals[visualSel.index];
      if (piece) return visualKindSheet(piece.kind);
    }
    return toolSheet(this.tool);
  }

  getAsset(sheet: SheetKey): HTMLImageElement | null {
    return this.assets[sheet];
  }

  setActiveSrc(sheet: SheetKey, src: Rect) {
    this.activeSrc[sheet] = cloneRect(src);
    this.onMapChange?.();
  }

  getSelectedVisual(): VisualPiece | null {
    if (this.selections.length !== 1 || this.selections[0].type !== "visual") {
      return null;
    }
    return this.map.visuals[this.selections[0].index] ?? null;
  }

  getSelectedShape(): MapShape | null {
    if (this.selections.length !== 1 || this.selections[0].type !== "shape") {
      return null;
    }
    return this.map.shapes[this.selections[0].index] ?? null;
  }

  updateSelectedShape(partial: Partial<MapShape>) {
    if (this.selections.length !== 1 || this.selections[0].type !== "shape") return;
    const shape = this.map.shapes[this.selections[0].index];
    if (!shape) return;
    if (shape.kind === "rect" && "cornerRadius" in partial && partial.cornerRadius !== undefined) {
      const r = clampCornerRadius(shape.rect, partial.cornerRadius);
      if (r > 0) shape.cornerRadius = r;
      else delete shape.cornerRadius;
      const rest = { ...partial };
      delete (rest as { cornerRadius?: number }).cornerRadius;
      Object.assign(shape, rest);
    } else {
      Object.assign(shape, partial);
    }
    this.onMapChange?.();
  }

  getSelectedCollider(): Collider | null {
    if (this.selections.length !== 1 || this.selections[0].type !== "collider") {
      return null;
    }
    return this.map.colliders[this.selections[0].index] ?? null;
  }

  updateSelectedCollider(partial: Partial<Collider>) {
    if (this.selections.length !== 1 || this.selections[0].type !== "collider") return;
    const collider = this.map.colliders[this.selections[0].index];
    if (!collider) return;
    if (
      collider.kind === "rect" &&
      "cornerRadius" in partial &&
      partial.cornerRadius !== undefined
    ) {
      const r = clampCornerRadius(collider.rect, partial.cornerRadius);
      if (r > 0) collider.cornerRadius = r;
      else delete collider.cornerRadius;
      const rest = { ...partial };
      delete (rest as { cornerRadius?: number }).cornerRadius;
      Object.assign(collider, rest);
    } else {
      Object.assign(collider, partial);
    }
    this.onMapChange?.();
  }

  updateSelectedVisual(partial: Partial<VisualPiece>) {
    if (this.selections.length !== 1 || this.selections[0].type !== "visual") return;
    const piece = this.map.visuals[this.selections[0].index];
    if (!piece) return;
    this.history.record(this.map);
    Object.assign(piece, partial);
    this.onMapChange?.();
  }

  selectionSupportsTag(): boolean {
    return this.selections.length === 1 && selectionSupportsTag(this.selections[0]);
  }

  getSelectionTag(): string | null {
    if (this.selections.length !== 1) return null;
    return objectTag(this.map, this.selections[0]);
  }

  setSelectionTag(tag: string) {
    if (this.selections.length !== 1 || !selectionSupportsTag(this.selections[0])) return;
    this.recordUndo();
    setObjectTag(this.map, this.selections[0], tag);
    this.setStatus("Updated tag");
    this.onMapChange?.();
  }

  private recordUndo() {
    this.history.record(this.map);
  }

  applyMapSettings(patch: MapSettingsPatch) {
    if (patch.gridSize !== undefined) {
      this.history.record(this.map);
      this.gridSize = Math.max(1, patch.gridSize);
      this.setStatus(`Grid size ${this.gridSize} (snap step ${this.snapStep()})`);
      this.onMapChange?.();
    }
    if (patch.background !== undefined) {
      this.applyBackground(patch.background);
    }
    if (patch.playAreaWidth !== undefined || patch.playAreaHeight !== undefined) {
      const pb = this.map.player_bounds;
      const width = patch.playAreaWidth ?? pb.w;
      const height = patch.playAreaHeight ?? pb.h;
      this.setPlayAreaSize(width, height);
    }
  }

  setPlayAreaSize(width: number, height: number) {
    const pb = this.map.player_bounds;
    const center = playAreaCenter(pb);
    this.recordUndo();
    this.map = applyFixedMapFrame({
      ...this.map,
      player_bounds: playAreaFromCenter(center.x, center.y, width, height),
    });
    this.setStatus(`Play area ${this.map.player_bounds.w}×${this.map.player_bounds.h}`);
    this.onMapChange?.();
  }

  getBackground(): MapBackground | null {
    return this.map.background ?? null;
  }

  applyBackground(background: MapBackground | null) {
    this.recordUndo();
    if (background && (background.color || background.image)) {
      const next: MapBackground = { ...background };
      if (!next.dest) next.dest = defaultBackgroundDest(this.map);
      this.map.background = next;
    } else {
      this.map.background = null;
    }
    void this.reloadBackgroundImage();
    this.setStatus(this.map.background ? "Background updated" : "Background cleared");
    this.onMapChange?.();
  }

  setBackgroundImageFile(file: File) {
    this.recordUndo();
    const path = `maps/backgrounds/${file.name}`;
    if (this.backgroundBlobUrl) {
      URL.revokeObjectURL(this.backgroundBlobUrl);
    }
    const url = URL.createObjectURL(file);
    this.backgroundBlobUrl = url;
    this.backgroundImageKey = path;
    const img = new Image();
    img.onload = () => {
      this.backgroundImage = img;
      const prev = this.map.background ?? {};
      this.map.background = {
        ...prev,
        image: path,
        dest: prev.dest ?? defaultBackgroundDest(this.map),
      };
      this.setStatus(`Background image loaded (${file.name})`);
      this.onMapChange?.();
    };
    img.onerror = () => {
      this.setStatus("Failed to load background image");
    };
    img.src = url;
  }

  fitBackgroundToCamera() {
    const bg = this.map.background;
    if (!bg) return;
    this.recordUndo();
    this.map.background = {
      ...bg,
      dest: defaultBackgroundDest(this.map),
    };
    this.setStatus("Background fitted to camera bounds");
    this.onMapChange?.();
  }

  private clearBackgroundImageCache() {
    if (this.backgroundBlobUrl) {
      URL.revokeObjectURL(this.backgroundBlobUrl);
    }
    this.backgroundBlobUrl = null;
    this.backgroundImage = null;
    this.backgroundImageKey = null;
  }

  private reloadBackgroundImage() {
    const path = this.map.background?.image;
    if (!path) {
      this.clearBackgroundImageCache();
      return;
    }
    if (this.backgroundImageKey === path && this.backgroundImage) {
      return;
    }
    this.clearBackgroundImageCache();
    const img = new Image();
    img.onload = () => {
      this.backgroundImage = img;
      this.backgroundImageKey = path;
      this.onMapChange?.();
    };
    img.onerror = () => {
      this.backgroundImage = null;
      this.backgroundImageKey = path;
    };
    img.src = backgroundImageUrl(path);
  }

  private async loadAssets() {
    const paths: [SheetKey, string][] = [
      ["ice", "/rink/ice.png"],
      ["walls", "/rink/walls.png"],
      ["circle", "/rink/circle.png"],
      ["goal", "/rink/goal.png"],
    ];
    const results = await Promise.all(
      paths.map(
        ([key, src]) =>
          new Promise<[SheetKey, boolean]>((resolve) => {
            const img = new Image();
            img.onload = () => {
              this.assets[key] = img;
              if (key === "ice" && !this.activeSrc.ice) {
                this.activeSrc.ice = {
                  x: 0,
                  y: 0,
                  w: img.width,
                  h: img.height,
                };
              }
              resolve([key, true]);
            };
            img.onerror = () => resolve([key, false]);
            img.src = src;
          }),
      ),
    );
    const missing = results.filter(([, ok]) => !ok).map(([key]) => key);
    if (missing.length > 0) {
      this.setStatus(`Missing preview art: ${missing.join(", ")} (see public/rink/)`);
    }
  }

  private bindCanvas() {
    this.canvas.addEventListener("wheel", (e) => {
      e.preventDefault();
      const factor = e.deltaY < 0 ? 1.1 : 1 / 1.1;
      this.zoom = Math.min(2.5, Math.max(0.08, this.zoom * factor));
    });

    this.canvas.addEventListener("mousedown", (e) => this.onPointerDown(e));
    this.canvas.addEventListener("contextmenu", (e) => e.preventDefault());
    this.canvas.addEventListener("mousemove", (e) => this.onPointerMove(e));
    this.canvas.addEventListener("mouseenter", () => {
      this.pointerInside = true;
    });
    this.canvas.addEventListener("mouseleave", () => {
      this.pointerInside = false;
      this.hoverCell = null;
      this.hoverSnapped = null;
    });
    window.addEventListener("mousemove", (e) => this.onPointerMove(e));
    window.addEventListener("mouseup", (e) => this.onPointerUp(e));
    window.addEventListener("keydown", (e) => this.onKeyDown(e));
    window.addEventListener("resize", () => this.resize());
    const ro = new ResizeObserver(() => this.resize());
    ro.observe(this.canvas.parentElement!);
    this.resize();
  }

  private resize() {
    const parent = this.canvas.parentElement!;
    const rect = parent.getBoundingClientRect();
    this.viewSize.w = Math.max(1, rect.width);
    this.viewSize.h = Math.max(1, rect.height);
    this.canvas.width = Math.floor(this.viewSize.w * devicePixelRatio);
    this.canvas.height = Math.floor(this.viewSize.h * devicePixelRatio);
  }

  /** Map a mouse event to view-space coords (matches the draw transform). */
  private pointerToView(e: MouseEvent): { sx: number; sy: number; inside: boolean } {
    const rect = this.canvas.getBoundingClientRect();
    const sx =
      ((e.clientX - rect.left) / Math.max(1, rect.width)) * this.viewSize.w;
    const sy =
      ((e.clientY - rect.top) / Math.max(1, rect.height)) * this.viewSize.h;
    const inside =
      e.clientX >= rect.left &&
      e.clientX <= rect.right &&
      e.clientY >= rect.top &&
      e.clientY <= rect.bottom;
    return { sx, sy, inside };
  }

  private viewToWorld(sx: number, sy: number): Point {
    return {
      x: (sx - this.viewSize.w * 0.5) / this.zoom + this.camX,
      y: (sy - this.viewSize.h * 0.5) / this.zoom + this.camY,
    };
  }

  private snapStep(): number {
    return this.gridSize * 8;
  }

  private gridCellAt(p: Point): { col: number; row: number } {
    const g = this.snapStep();
    return { col: gridCellIndex(p.x, g), row: gridCellIndex(p.y, g) };
  }

  private snapPoint(p: Point): Point {
    return snapPointToTile(p, this.snapStep());
  }

  private snapCoord(v: number): number {
    return snapScalarToGridAnchors(v, this.snapStep());
  }

  private snapRectToGrid(r: Rect): Rect {
    const g = this.snapStep();
    const half = g * 0.5;
    const x0 = this.snapCoord(r.x);
    const y0 = this.snapCoord(r.y);
    const x1 = this.snapCoord(r.x + r.w);
    const y1 = this.snapCoord(r.y + r.h);
    return {
      x: x0,
      y: y0,
      w: Math.max(half, x1 - x0),
      h: Math.max(half, y1 - y0),
    };
  }

  private handleHitRadius(): number {
    return 10 / this.zoom;
  }

  private isClickShape(shape: DrawShape = this.effectiveShape()): boolean {
    return CLICK_SHAPES.includes(shape);
  }

  private fitCamera() {
    const pb = this.map.player_bounds;
    this.camX = pb.x + pb.w * 0.5;
    this.camY = pb.y + pb.h * 0.5;
    this.zoom = 0.45;
  }

  private onPointerDown(e: MouseEvent) {
    const { sx, sy } = this.pointerToView(e);
    const world = this.viewToWorld(sx, sy);

    if (e.button === 1 || (e.button === 0 && e.shiftKey)) {
      this.dragMode = "pan";
      this.dragStartWorld = { x: sx, y: sy };
      return;
    }

    if (e.button === 2) {
      e.preventDefault();
      if (this.tool === "select") {
        this.selectThroughStack(world);
      }
      return;
    }

    if (e.button !== 0) return;

    this.resetPickThrough();

    if (this.tool === "select") {
      if (this.selections.length === 1) {
        const sole = this.selections[0];
        const handle = this.hitTestEditHandle(world.x, world.y, sole);
        if (handle && !isObjectLocked(this.map, sole)) {
          this.recordUndo();
          this.dragMode = "point";
          this.activeHandle = handle;
          this.onMapChange?.();
          return;
        }
      }

      const hit = this.hitTest(world.x, world.y);
      const mod = e.ctrlKey || e.metaKey;

      if (!hit) {
        if (!mod) this.selections = [];
        this.dragMode = "marquee";
        this.marqueeStartWorld = world;
        this.marqueeRect = null;
        this.onMapChange?.();
        return;
      }

      const inSelection = this.selections.some((s) => selectionEquals(s, hit));
      if (mod) {
        this.toggleSelection(hit);
        this.onMapChange?.();
        return;
      }
      if (e.shiftKey) {
        if (!inSelection) this.selections = [...this.selections, hit];
      } else if (!inSelection) {
        this.selections = [hit];
      }

      const movable = this.selections.filter((sel) => !isObjectLocked(this.map, sel));
      if (movable.length > 0) {
        this.recordUndo();
        this.dragMode = "move";
        this.dragStartWorld = world;
        this.dragStartSnapped = this.snapPoint(world);
        this.initDragMoveStates();
      } else {
        this.setStatus(`${this.describeSelection(hit)} (locked)`);
      }
      this.onMapChange?.();
      return;
    }

    const shape = this.effectiveShape();

    if (this.isClickShape(shape)) {
      if (this.clickPoints.length === 0) this.recordUndo();
      const pt = this.snapPoint(world);
      this.clickPoints.push(pt);
      const needed = shape === "arc" ? 3 : 2;
      this.setStatus(`${this.tool} · ${shape}: point ${this.clickPoints.length}/${needed}`);
      if (this.clickPoints.length >= needed) {
        this.commitClickShape(shape);
        this.clickPoints = [];
        this.updateToolStatus();
        this.onMapChange?.();
      }
      return;
    }

    this.recordUndo();
    this.dragMode = "draw";
    this.dragStartWorld = this.snapPoint(world);
    this.previewRect = null;
    this.previewCircle = null;
    this.previewLine = null;
  }

  private onPointerMove(e: MouseEvent) {
    const { sx, sy, inside } = this.pointerToView(e);
    if (inside || this.dragMode === "pan") {
      this.pointerInside = inside;
    }
    const world = this.viewToWorld(sx, sy);
    if (this.pointerInside && this.dragMode !== "pan") {
      const step = this.snapStep();
      this.hoverCell = this.gridCellAt(world);
      this.hoverSnapped = this.snapPoint(world);
    }

    if (this.dragMode === "pan") {
      this.camX -= (sx - this.dragStartWorld.x) / this.zoom;
      this.camY -= (sy - this.dragStartWorld.y) / this.zoom;
      this.dragStartWorld = { x: sx, y: sy };
      return;
    }

    if (this.dragMode === "point" && this.activeHandle) {
      this.applyEditHandle(this.activeHandle, this.snapPoint(world));
      return;
    }

    if (this.dragMode === "move" && this.selections.length > 0 && this.dragStartSnapped) {
      const cur = this.snapPoint(world);
      const dx = cur.x - this.dragStartSnapped.x;
      const dy = cur.y - this.dragStartSnapped.y;
      for (const state of this.dragMoveStates) {
        if (isObjectLocked(this.map, state.sel)) continue;
        if (state.arc) {
          this.applyArcMove(state.sel, translateArc(state.arc, dx, dy));
        } else if (state.line) {
          this.applyLineMove(state.sel, {
            p1: {
              x: state.line.p1.x + dx,
              y: state.line.p1.y + dy,
            },
            p2: {
              x: state.line.p2.x + dx,
              y: state.line.p2.y + dy,
            },
          });
        } else if (state.rect) {
          const moved = {
            ...state.rect,
            x: state.rect.x + dx,
            y: state.rect.y + dy,
          };
          this.applyRect(state.sel, moved);
        }
      }
      return;
    }

    if (this.dragMode === "marquee") {
      this.marqueeRect = rectFromPoints(this.marqueeStartWorld, world);
      return;
    }

    const shape = this.effectiveShape();

    if (this.isClickShape(shape) && this.clickPoints.length > 0 && this.hoverSnapped) {
      const pts = [...this.clickPoints, this.hoverSnapped];
      if (shape === "line" && pts.length >= 2) {
        this.previewLine = { p1: pts[0], p2: pts[1] };
      }
      if (shape === "arc" && pts.length >= 2) {
        if (pts.length === 2) {
          this.previewLine = { p1: pts[0], p2: pts[1] };
        } else {
          this.previewLine = null;
        }
      }
    }

    if (this.dragMode === "draw") {
      if (shape === "circle") {
        const g = this.snapStep();
        const snapped = this.snapPoint(world);
        const r = Math.max(
          g * 0.5,
          dist(this.dragStartWorld.x, this.dragStartWorld.y, snapped.x, snapped.y),
        );
        this.previewCircle = { x: this.dragStartWorld.x, y: this.dragStartWorld.y, r };
        this.previewRect = null;
        return;
      }

      this.previewRect = rectFromSnapAnchors(
        this.dragStartWorld,
        world,
        this.snapStep(),
      );
    }
  }

  private onPointerUp(e?: MouseEvent) {
    if (this.dragMode === "marquee" && this.marqueeRect) {
      const minSize = this.snapStep() * 0.25;
      if (this.marqueeRect.w >= minSize || this.marqueeRect.h >= minSize) {
        const hits = this.collectSelectionsInRect(this.marqueeRect);
        const mod = e?.ctrlKey || e?.metaKey;
        if (mod) {
          const merged = [...this.selections];
          for (const hit of hits) {
            if (!merged.some((s) => selectionEquals(s, hit))) {
              merged.push(hit);
            }
          }
          this.selections = merged;
        } else {
          this.selections = hits;
        }
        this.setStatus(
          this.selections.length === 0
            ? "No objects in selection"
            : `Selected ${this.selections.length} object${this.selections.length === 1 ? "" : "s"}`,
        );
        this.onMapChange?.();
      }
    }

    if (this.dragMode === "draw") {
      const shape = this.effectiveShape();
      const commit = this.buildDrawCommit(shape);
      if (commit?.circle) {
        this.commitCircle(commit.circle);
        this.onMapChange?.();
      } else if (commit?.rect) {
        this.commitRect(commit.rect, shape);
        this.onMapChange?.();
      }
      this.previewRect = null;
      this.previewCircle = null;
    }
    if (this.dragMode === "point") {
      this.onMapChange?.();
    }

    if (this.dragMode === "move") {
      this.snapSelectedGeometryToGrid({ skipRects: true });
      this.onMapChange?.();
    }

    this.dragMode = null;
    this.marqueeRect = null;
    this.dragMoveStates = [];
    this.dragStartSnapped = null;
    this.activeHandle = null;
  }

  private onKeyDown(e: KeyboardEvent) {
    if (
      e.target instanceof HTMLInputElement ||
      e.target instanceof HTMLTextAreaElement
    ) {
      return;
    }
    const mod = e.ctrlKey || e.metaKey;
    if (mod && e.key.toLowerCase() === "z" && e.shiftKey) {
      e.preventDefault();
      this.redo();
      return;
    }
    if (mod && e.key.toLowerCase() === "z") {
      e.preventDefault();
      this.undo();
      return;
    }
    if (e.key === "Delete" || e.key === "Backspace") {
      this.deleteSelected();
      return;
    }
    if (this.selectionHasZOrder()) {
      if (e.key === "]") {
        e.preventDefault();
        if (mod) this.bringSelectionToFront();
        else this.nudgeSelectionZ(1);
        return;
      }
      if (e.key === "[") {
        e.preventDefault();
        if (mod) this.sendSelectionToBack();
        else this.nudgeSelectionZ(-1);
        return;
      }
    }
  }

  deleteSelected() {
    if (this.selections.length === 0) return;
    const deletable = this.selections.filter((sel) => !isObjectLocked(this.map, sel));
    if (deletable.length === 0) {
      this.setStatus("Selection is locked");
      return;
    }
    this.recordUndo();

    const visualIdx = deletable
      .filter((s): s is { type: "visual"; index: number } => s.type === "visual")
      .map((s) => s.index)
      .sort((a, b) => b - a);
    for (const i of visualIdx) {
      this.map.visuals.splice(i, 1);
    }

    const shapeIdx = deletable
      .filter((s): s is { type: "shape"; index: number } => s.type === "shape")
      .map((s) => s.index)
      .sort((a, b) => b - a);
    for (const i of shapeIdx) {
      this.map.shapes.splice(i, 1);
    }

    const colliderIdx = deletable
      .filter((s): s is { type: "collider"; index: number } => s.type === "collider")
      .map((s) => s.index)
      .sort((a, b) => b - a);
    for (const i of colliderIdx) {
      this.map.colliders.splice(i, 1);
    }

    for (const sel of deletable) {
      switch (sel.type) {
        case "goal_west":
          this.map.goal_west = null;
          break;
        case "goal_east":
          this.map.goal_east = null;
          break;
        case "spawn":
          this.clearSpawn(sel.key);
          break;
      }
    }

    const skipped = this.selections.length - deletable.length;
    this.selections = [];
    if (skipped > 0) {
      this.setStatus(
        `Deleted ${deletable.length} object${deletable.length === 1 ? "" : "s"} (${skipped} locked skipped)`,
      );
    } else {
      this.setStatus(`Deleted ${deletable.length} object${deletable.length === 1 ? "" : "s"}`);
    }
    this.onMapChange?.();
  }

  clearSelection() {
    if (this.selections.length === 0) return;
    this.selections = [];
    this.setStatus("Cleared selection");
    this.onMapChange?.();
  }

  listMapObjects(): MapObjectListEntry[] {
    return listMapObjects(this.map);
  }

  isSelected(sel: Selection): boolean {
    return this.selections.some((s) => selectionEquals(s, sel));
  }

  selectFromList(sel: Selection, opts?: { add?: boolean; toggle?: boolean }) {
    if (opts?.toggle) {
      this.toggleSelection(sel);
    } else if (opts?.add) {
      if (!this.isSelected(sel)) {
        this.selections = [...this.selections, sel];
      }
    } else {
      this.selections = [sel];
    }
    const label = this.describeSelection(sel);
    const locked = isObjectLocked(this.map, sel) ? " (locked)" : "";
    this.setStatus(`Selected ${label}${locked}`);
    this.onMapChange?.();
  }

  selectionSupportsLock(): boolean {
    return this.selections.some((sel) => selectionSupportsLock(sel));
  }

  getSelectionLockState(): "none" | "all" | "mixed" | "unsupported" {
    const lockable = this.selections.filter((sel) => selectionSupportsLock(sel));
    if (lockable.length === 0) return "unsupported";
    const lockedCount = lockable.filter((sel) => isObjectLocked(this.map, sel)).length;
    if (lockedCount === 0) return "none";
    if (lockedCount === lockable.length) return "all";
    return "mixed";
  }

  toggleSelectionLock() {
    const lockable = this.selections.filter((sel) => selectionSupportsLock(sel));
    if (lockable.length === 0) return;
    const lock = lockable.some((sel) => !isObjectLocked(this.map, sel));
    this.recordUndo();
    for (const sel of lockable) {
      setObjectLocked(this.map, sel, lock);
    }
    this.setStatus(lock ? "Locked selection" : "Unlocked selection");
    this.onMapChange?.();
  }

  private editableSelections(sels: Selection[]): Selection[] {
    return sels.filter((sel) => !isObjectLocked(this.map, sel));
  }

  selectionHasZOrder(): boolean {
    return selectionHasZOrder(this.selections);
  }

  /** Effective z when all selected z-capable items share one value; otherwise null. */
  selectionZ(): number | null {
    const sels = zCapableSelections(this.selections);
    if (sels.length === 0) return null;
    const values = sels.map((sel) => effectiveSelectionZ(this.map, sel)!);
    return values.every((z) => z === values[0]) ? values[0] : null;
  }

  setSelectionZ(z: number) {
    const sels = this.editableSelections(zCapableSelections(this.selections));
    if (sels.length === 0) return;
    this.recordUndo();
    materializeDrawableZ(this.map);
    for (const sel of sels) {
      setSelectionZValue(this.map, sel, Math.round(z));
    }
    this.setStatus(`Set layer z to ${Math.round(z)}`);
    this.onMapChange?.();
  }

  nudgeSelectionZ(delta: number) {
    const sels = this.editableSelections(zCapableSelections(this.selections));
    if (sels.length === 0 || delta === 0) return;
    this.recordUndo();
    materializeDrawableZ(this.map);
    for (const sel of sels) {
      const entries = buildDrawableOrder(this.map);
      const key = selectionToDrawableKey(sel);
      if (!key) continue;
      const idx = entries.findIndex((entry) => drawableEntryKey(entry) === key);
      if (idx < 0) continue;
      const targetIdx = idx + (delta > 0 ? 1 : -1);
      if (targetIdx < 0 || targetIdx >= entries.length) continue;
      const curZ = entries[idx].z;
      const otherZ = entries[targetIdx].z;
      setSelectionZValue(this.map, sel, otherZ);
      const other = entries[targetIdx];
      if (other.kind === "visual") {
        this.map.visuals[other.index].z = curZ;
      } else {
        this.map.shapes[other.index].z = curZ;
      }
    }
    this.setStatus(delta > 0 ? "Moved layer forward" : "Moved layer backward");
    this.onMapChange?.();
  }

  bringSelectionToFront() {
    const sels = this.editableSelections(zCapableSelections(this.selections));
    if (sels.length === 0) return;
    this.recordUndo();
    materializeDrawableZ(this.map);
    let next =
      Math.max(...buildDrawableOrder(this.map).map((entry) => entry.z), 0) + 1;
    for (const sel of sels) {
      setSelectionZValue(this.map, sel, next);
      next += 1;
    }
    this.setStatus("Moved to front");
    this.onMapChange?.();
  }

  sendSelectionToBack() {
    const sels = this.editableSelections(zCapableSelections(this.selections));
    if (sels.length === 0) return;
    this.recordUndo();
    materializeDrawableZ(this.map);
    let next =
      Math.min(...buildDrawableOrder(this.map).map((entry) => entry.z), 0) - 1;
    for (const sel of sels) {
      setSelectionZValue(this.map, sel, next);
      next -= 1;
    }
    this.setStatus("Moved to back");
    this.onMapChange?.();
  }

  selectionSummary(): string {
    if (this.selections.length === 0) return "Nothing selected";
    if (this.selections.length === 1) {
      return this.describeSelection(this.selections[0]);
    }
    const counts = new Map<string, number>();
    for (const sel of this.selections) {
      const key =
        sel.type === "visual"
          ? "visuals"
          : sel.type === "shape"
            ? "shapes"
            : sel.type === "collider"
              ? "colliders"
              : sel.type === "spawn"
                ? "spawns"
                : sel.type;
      counts.set(key, (counts.get(key) ?? 0) + 1);
    }
    const parts = [...counts.entries()].map(([k, n]) => `${n} ${k}`);
    return `${this.selections.length} selected (${parts.join(", ")})`;
  }

  private describeSelection(sel: Selection): string {
    const label = objectTag(this.map, sel);
    return isObjectLocked(this.map, sel) ? `${label} (locked)` : label;
  }

  private goalRectZone(side: "west" | "east", rect: Rect): GoalZone {
    const existing = side === "west" ? this.map.goal_west : this.map.goal_east;
    return { kind: "rect", rect, tag: existing?.tag ?? defaultGoalTag(side) };
  }

  private goalArcZone(side: "west" | "east", arc: Arc3): GoalZone {
    const existing = side === "west" ? this.map.goal_west : this.map.goal_east;
    return { kind: "arc", arc, tag: existing?.tag ?? defaultGoalTag(side) };
  }

  private setSpawn(key: SpawnKey, point: Point | null) {
    setSpawnPoint(this.map.spawns, key, point);
  }

  private clearSpawn(key: SpawnKey) {
    this.setSpawn(key, null);
  }

  private isVisualTool(tool: Tool): boolean {
    return (
      tool === "ice_tile" ||
      tool === "wall_frame" ||
      tool === "circle_mark" ||
      tool === "goal_sprite"
    );
  }

  private srcForTool(tool: Tool): Rect | undefined {
    if (!this.useTileSrc) return undefined;
    const sheet = toolSheet(tool);
    if (!sheet) return undefined;
    return this.activeSrc[sheet];
  }

  private commitClickShape(shape: DrawShape) {
    if (shape === "line") {
      const line = { p1: this.clickPoints[0], p2: this.clickPoints[1] };
      if (this.tool === "collider") {
        this.map.colliders.push({ kind: "line", ...line, tag: defaultColliderTag("line") });
        this.applyLineArcTangentJoins();
        this.setStatus("Placed line collider (tangent arc join if connected)");
        return;
      }
      if (this.tool === "color_shape") {
        this.map.shapes.push({
          kind: "line",
          ...line,
          tag: defaultShapeTag("line"),
          ...this.activeDrawStyle(),
        });
        this.setStatus("Placed color line");
        return;
      }
      this.setStatus("Line shape not available for this tool");
      return;
    }

    if (shape === "arc") {
      const arc = {
        p1: this.clickPoints[0],
        p2: this.clickPoints[1],
        p3: this.clickPoints[2],
      };
      if (!arcFrom3Points(arc)) {
        this.setStatus("Invalid arc (points are nearly collinear)");
        return;
      }
      if (this.tool === "color_shape") {
        this.map.shapes.push({
          kind: "arc",
          arc,
          tag: defaultShapeTag("arc"),
          ...this.activeDrawStyle(),
        });
        this.setStatus("Placed color arc");
        return;
      }
      if (this.tool === "collider") {
        this.map.colliders.push({ kind: "arc", arc, tag: defaultColliderTag("arc") });
      } else if (this.tool === "goal_west") {
        this.map.goal_west = this.goalArcZone("west", arc);
      } else if (this.tool === "goal_east") {
        this.map.goal_east = this.goalArcZone("east", arc);
      }
      this.setStatus(`Placed ${this.tool} arc`);
    }
  }

  private commitCircle(c: { x: number; y: number; r: number }) {
    const r = c.r;
    const rect: Rect = { x: c.x - r, y: c.y - r, w: r * 2, h: r * 2 };
    if (this.tool === "collider") {
      this.map.colliders.push({
        kind: "circle",
        x: c.x,
        y: c.y,
        radius: r,
        tag: defaultColliderTag("circle"),
      });
      this.setStatus("Placed circle collider");
      return;
    }
    if (this.tool === "color_shape") {
      this.map.shapes.push({
        kind: "circle",
        x: c.x,
        y: c.y,
        radius: r,
        tag: defaultShapeTag("circle"),
        ...this.activeDrawStyle(),
      });
      this.setStatus("Placed color circle");
      return;
    }
    this.commitRect(rect, "circle");
  }

  private commitRect(r: Rect, shape: DrawShape = "square") {
    const rect = this.snapRectToGrid(r);
    const src = this.srcForTool(this.tool);
    if (this.tool === "color_shape") {
      const cornerRadius = this.rectCornerRadiusForNew(rect);
      this.map.shapes.push({
        kind: "rect",
        rect,
        tag: defaultShapeTag("rect"),
        ...this.activeDrawStyle(),
        ...(cornerRadius ? { cornerRadius } : {}),
      });
      this.setStatus("Placed color rect");
      return;
    }
    switch (this.tool) {
      case "ice_tile":
        this.map.visuals.push({
          id: uid("ice"),
          kind: "ice_tile",
          dest: rect,
          tag: defaultVisualTag("ice_tile"),
          ...(src ? { use_src: true, src: cloneRect(src) } : {}),
        });
        break;
      case "wall_frame":
        this.map.visuals.push({
          id: uid("wall"),
          kind: "wall_frame",
          dest: rect,
          tag: defaultVisualTag("wall_frame"),
          ...WALL_SLICES,
          slice_l: WALL_SLICES.l,
          slice_t: WALL_SLICES.t,
          slice_r: WALL_SLICES.r,
          slice_b: WALL_SLICES.b,
          ...(src ? { use_src: true, src: cloneRect(src) } : {}),
        });
        break;
      case "circle_mark":
        this.map.visuals.push({
          id: uid("circle"),
          kind: "circle_mark",
          dest: rect,
          tag: defaultVisualTag("circle_mark"),
          use_src: true,
          src: cloneRect(src ?? CIRCLE_SRC),
        });
        break;
      case "goal_sprite":
        this.map.visuals.push({
          id: uid("goal"),
          kind: "goal_sprite",
          dest: rect,
          tag: defaultVisualTag("goal_sprite"),
          ...(src ? { use_src: true, src: cloneRect(src) } : {}),
        });
        break;
      case "collider": {
        const cornerRadius = this.rectCornerRadiusForNew(rect);
        this.map.colliders.push({
          kind: "rect",
          rect,
          tag: defaultColliderTag("rect"),
          ...(cornerRadius ? { cornerRadius } : {}),
        });
        break;
      }
      case "goal_west":
        this.map.goal_west = this.goalRectZone("west", rect);
        break;
      case "goal_east":
        this.map.goal_east = this.goalRectZone("east", rect);
        break;
      default: {
        const spawnKey = spawnToolToKey(this.tool);
        if (spawnKey) {
          const p = { x: rect.x + rect.w * 0.5, y: rect.y + rect.h * 0.5 };
          setSpawnPoint(this.map.spawns, spawnKey, p);
          if (spawnKey === "puck_start") {
            this.map.spawns.puck_faceoff = { ...p };
          }
        }
        break;
      }
    }
    void shape;
    this.setStatus(`Placed ${this.tool}`);
  }

  private commitPlacement(r: Rect) {
    this.commitRect(r, "square");
  }

  private buildDrawCommit(
    shape: DrawShape,
  ): { rect?: Rect; circle?: { x: number; y: number; r: number } } | null {
    if (this.isClickShape(shape)) return null;

    const g = this.snapStep();
    if (shape === "circle") {
      if (this.previewCircle) return { circle: this.previewCircle };
      return {
        circle: {
          x: this.dragStartWorld.x,
          y: this.dragStartWorld.y,
          r: g * 0.5,
        },
      };
    }

    if (this.previewRect) return { rect: this.snapRectToGrid(this.previewRect) };
    const half = g * 0.5;
    return {
      rect: this.snapRectToGrid({
        x: this.dragStartWorld.x,
        y: this.dragStartWorld.y,
        w: half,
        h: half,
      }),
    };
  }

  private resetPickThrough() {
    this.pickThroughPoint = null;
    this.pickThroughHits = [];
    this.pickThroughIndex = 0;
  }

  private selectThroughStack(world: Point) {
    const hits = this.hitTestAll(world.x, world.y);
    if (hits.length === 0) {
      this.resetPickThrough();
      this.selections = [];
      this.setStatus("Nothing under cursor");
      this.onMapChange?.();
      return;
    }

    const threshold = 8 / this.zoom;
    const samePoint =
      this.pickThroughPoint !== null &&
      dist(world.x, world.y, this.pickThroughPoint.x, this.pickThroughPoint.y) <= threshold;
    const sameHits =
      samePoint &&
      hits.length === this.pickThroughHits.length &&
      hits.every((hit, i) => selectionEquals(hit, this.pickThroughHits[i]!));

    if (!sameHits) {
      this.pickThroughPoint = { x: world.x, y: world.y };
      this.pickThroughHits = hits;
      this.pickThroughIndex = 0;
    } else {
      this.pickThroughIndex = (this.pickThroughIndex + 1) % hits.length;
    }

    const pick = hits[this.pickThroughIndex]!;
    this.selections = [pick];
    const label = this.describeSelection(pick);
    if (hits.length === 1) {
      this.setStatus(`Selected ${label}`);
    } else {
      this.setStatus(
        `Selected ${label} (${this.pickThroughIndex + 1}/${hits.length} under cursor — right-click to cycle)`,
      );
    }
    this.onMapChange?.();
  }

  private hitTestGoalZone(x: number, y: number, side: "west" | "east"): boolean {
    const zone = side === "west" ? this.map.goal_west : this.map.goal_east;
    if (!zone) return false;
    if (zone.kind === "rect") return pointInRect(x, y, zone.rect);
    return hitTestArc(x, y, zone.arc, 12 / this.zoom);
  }

  private hitTestShapeAt(index: number, x: number, y: number, threshold: number): boolean {
    const s = this.map.shapes[index];
    if (!s) return false;
    if (s.kind === "rect") {
      const cr = s.cornerRadius ?? 0;
      if (cr > 0 && pointInRoundRect(x, y, s.rect, cr)) return true;
      return pointInRect(x, y, s.rect);
    }
    if (s.kind === "circle") return dist(x, y, s.x, s.y) <= s.radius;
    if (s.kind === "arc") return hitTestArc(x, y, s.arc, threshold);
    if (s.kind === "line") return hitTestLine(x, y, s.p1, s.p2, threshold);
    return false;
  }

  private hitTestAll(x: number, y: number): Selection[] {
    const out: Selection[] = [];
    const cellCenter = this.snapPoint({ x, y });
    const threshold = 12 / this.zoom;

    const entries = buildDrawableOrder(this.map);
    for (let i = entries.length - 1; i >= 0; i--) {
      const entry = entries[i];
      if (entry.kind === "visual") {
        const piece = this.map.visuals[entry.index];
        if (piece && pointInRect(cellCenter.x, cellCenter.y, piece.dest)) {
          out.push({ type: "visual", index: entry.index });
        }
      } else if (this.hitTestShapeAt(entry.index, x, y, threshold)) {
        out.push({ type: "shape", index: entry.index });
      }
    }

    for (let i = this.map.colliders.length - 1; i >= 0; i--) {
      const c = this.map.colliders[i];
      if (c.kind === "rect") {
        const cr = c.cornerRadius ?? 0;
        if (cr > 0 && pointInRoundRect(x, y, c.rect, cr)) {
          out.push({ type: "collider", index: i });
          continue;
        }
        if (pointInRect(x, y, c.rect)) out.push({ type: "collider", index: i });
      } else if (c.kind === "circle" && dist(x, y, c.x, c.y) <= c.radius) {
        out.push({ type: "collider", index: i });
      } else if (c.kind === "arc" && hitTestArc(x, y, c.arc, threshold)) {
        out.push({ type: "collider", index: i });
      } else if (c.kind === "line" && hitTestLine(x, y, c.p1, c.p2, threshold)) {
        out.push({ type: "collider", index: i });
      }
    }

    if (this.hitTestGoalZone(x, y, "west")) out.push({ type: "goal_west" });
    if (this.hitTestGoalZone(x, y, "east")) out.push({ type: "goal_east" });

    const spawnRadius = 14 / this.zoom;
    for (const [key, p] of allSpawnChecks(this.map.spawns)) {
      if (p && dist(x, y, p.x, p.y) <= spawnRadius) {
        out.push({ type: "spawn", key });
      }
    }

    return out;
  }

  private hitTest(x: number, y: number): Selection | null {
    return this.hitTestAll(x, y)[0] ?? null;
  }

  private toggleSelection(hit: Selection) {
    const idx = this.selections.findIndex((s) => selectionEquals(s, hit));
    if (idx >= 0) {
      this.selections = this.selections.filter((_, i) => i !== idx);
      return;
    }
    this.selections = [...this.selections, hit];
  }

  private initDragMoveStates() {
    this.dragMoveStates = this.selections.map((sel) => ({
      sel,
      rect: this.selectionRect(sel),
      arc: this.selectionArc(sel),
      line: this.selectionLine(sel),
    }));
  }

  private collectSelectionsInRect(r: Rect): Selection[] {
    const out: Selection[] = [];
    for (let i = 0; i < this.map.visuals.length; i++) {
      if (rectsIntersect(r, this.map.visuals[i].dest)) {
        out.push({ type: "visual", index: i });
      }
    }
    for (let i = 0; i < this.map.shapes.length; i++) {
      const bounds = this.selectionRect({ type: "shape", index: i });
      if (bounds && rectsIntersect(r, bounds)) {
        out.push({ type: "shape", index: i });
      }
    }
    for (let i = 0; i < this.map.colliders.length; i++) {
      const bounds = this.selectionRect({ type: "collider", index: i });
      if (bounds && rectsIntersect(r, bounds)) {
        out.push({ type: "collider", index: i });
      }
    }
    if (this.map.goal_west) {
      const bounds = this.goalZoneBounds(this.map.goal_west);
      if (bounds && rectsIntersect(r, bounds)) {
        out.push({ type: "goal_west" });
      }
    }
    if (this.map.goal_east) {
      const bounds = this.goalZoneBounds(this.map.goal_east);
      if (bounds && rectsIntersect(r, bounds)) {
        out.push({ type: "goal_east" });
      }
    }
    const s = this.snapStep();
    for (const [key, p] of allSpawnChecks(this.map.spawns)) {
      if (!p) continue;
      const bounds: Rect = { x: p.x - s * 0.5, y: p.y - s * 0.5, w: s, h: s };
      if (rectsIntersect(r, bounds)) {
        out.push({ type: "spawn", key });
      }
    }
    return out;
  }

  private selectionLine(sel: Selection): { p1: Point; p2: Point } | null {
    if (sel.type === "collider") {
      const c = this.map.colliders[sel.index];
      if (c?.kind === "line") return { p1: c.p1, p2: c.p2 };
      return null;
    }
    if (sel.type === "shape") {
      const s = this.map.shapes[sel.index];
      if (s?.kind === "line") return { p1: s.p1, p2: s.p2 };
    }
    return null;
  }

  private selectionArc(sel: Selection): Arc3 | null {
    if (sel.type === "collider") {
      const c = this.map.colliders[sel.index];
      if (c?.kind === "arc") return c.arc;
    }
    if (sel.type === "shape") {
      const s = this.map.shapes[sel.index];
      if (s?.kind === "arc") return s.arc;
    }
    if (sel.type === "goal_west" && this.map.goal_west?.kind === "arc") {
      return this.map.goal_west.arc;
    }
    if (sel.type === "goal_east" && this.map.goal_east?.kind === "arc") {
      return this.map.goal_east.arc;
    }
    return null;
  }

  private selectionRect(sel: Selection): Rect | null {
    switch (sel.type) {
      case "visual":
        return this.map.visuals[sel.index]?.dest ?? null;
      case "shape": {
        const s = this.map.shapes[sel.index];
        if (!s) return null;
        if (s.kind === "rect") return s.rect;
        if (s.kind === "arc") return boundsOfArc(s.arc);
        if (s.kind === "line") return lineBounds(s.p1, s.p2);
        return {
          x: s.x - s.radius,
          y: s.y - s.radius,
          w: s.radius * 2,
          h: s.radius * 2,
        };
      }
      case "collider": {
        const c = this.map.colliders[sel.index];
        if (!c) return null;
        if (c.kind === "rect") return c.rect;
        if (c.kind === "arc") return boundsOfArc(c.arc);
        if (c.kind === "line") return lineBounds(c.p1, c.p2);
        return {
          x: c.x - c.radius,
          y: c.y - c.radius,
          w: c.radius * 2,
          h: c.radius * 2,
        };
      }
      case "goal_west":
        return this.goalZoneBounds(this.map.goal_west);
      case "goal_east":
        return this.goalZoneBounds(this.map.goal_east);
      case "spawn": {
        const p = this.spawnPoint(sel.key);
        if (!p) return null;
        const s = this.snapStep();
        return { x: p.x - s * 0.5, y: p.y - s * 0.5, w: s, h: s };
      }
      default:
        return null;
    }
  }

  private goalZoneBounds(zone: GoalZone | null): Rect | null {
    if (!zone) return null;
    if (zone.kind === "rect") return zone.rect;
    return boundsOfArc(zone.arc);
  }

  private spawnPoint(key: SpawnKey): Point | null {
    return getSpawnPoint(this.map.spawns, key);
  }

  private applyLineMove(sel: Selection, line: { p1: Point; p2: Point }) {
    if (sel.type === "collider") {
      const c = this.map.colliders[sel.index];
      if (c?.kind === "line") {
        c.p1 = line.p1;
        c.p2 = line.p2;
        this.applyLineArcTangentJoins();
      }
      return;
    }
    if (sel.type === "shape") {
      const s = this.map.shapes[sel.index];
      if (s?.kind === "line") {
        s.p1 = line.p1;
        s.p2 = line.p2;
      }
    }
  }

  /** When a line endpoint meets an arc endpoint, adjust arc p3 for a tangent join. */
  private applyLineArcTangentJoins() {
    const eps = Math.max(0.5, this.snapStep() * 0.02);
    for (const lineCol of this.map.colliders) {
      if (lineCol.kind !== "line") continue;
      const dir = lineDirection(lineCol);
      if (!dir) continue;
      for (const arcCol of this.map.colliders) {
        if (arcCol.kind !== "arc") continue;
        for (const junction of ["p1", "p2"] as const) {
          const J = junction === "p1" ? arcCol.arc.p1 : arcCol.arc.p2;
          const atP1 = pointsNear(lineCol.p1, J, eps);
          const atP2 = pointsNear(lineCol.p2, J, eps);
          if (!atP1 && !atP2) continue;
          const adjusted = arcWithTangentAtEndpoint(
            arcCol.arc,
            junction,
            dir,
            arcCol.arc.p3,
          );
          if (adjusted) arcCol.arc = adjusted;
        }
      }
    }
  }

  private applyArcMove(sel: Selection, arc: Arc3) {
    if (sel.type === "collider") {
      const c = this.map.colliders[sel.index];
      if (c?.kind === "arc") c.arc = arc;
    } else if (sel.type === "shape") {
      const s = this.map.shapes[sel.index];
      if (s?.kind === "arc") s.arc = arc;
    } else if (sel.type === "goal_west" && this.map.goal_west?.kind === "arc") {
      this.map.goal_west = this.goalArcZone("west", arc);
    } else if (sel.type === "goal_east" && this.map.goal_east?.kind === "arc") {
      this.map.goal_east = this.goalArcZone("east", arc);
    }
  }

  private applyRect(sel: Selection, r: Rect) {
    switch (sel.type) {
      case "visual":
        this.map.visuals[sel.index].dest = r;
        break;
      case "shape": {
        const s = this.map.shapes[sel.index];
        if (!s) break;
        if (s.kind === "rect") s.rect = r;
        else if (s.kind === "circle") {
          s.x = r.x + r.w * 0.5;
          s.y = r.y + r.h * 0.5;
          s.radius = Math.max(r.w, r.h) * 0.5;
        }
        this.clampStoredRectCornerRadius(sel);
        break;
      }
      case "collider": {
        const c = this.map.colliders[sel.index];
        if (c.kind === "rect") c.rect = r;
        else if (c.kind === "circle") {
          c.x = r.x + r.w * 0.5;
          c.y = r.y + r.h * 0.5;
          c.radius = Math.max(r.w, r.h) * 0.5;
        }
        this.clampStoredRectCornerRadius(sel);
        break;
      }
      case "goal_west":
        if (this.map.goal_west?.kind === "rect") {
          this.map.goal_west = this.goalRectZone("west", r);
        }
        break;
      case "goal_east":
        if (this.map.goal_east?.kind === "rect") {
          this.map.goal_east = this.goalRectZone("east", r);
        }
        break;
      case "spawn": {
        const center = { x: r.x + r.w * 0.5, y: r.y + r.h * 0.5 };
        this.setSpawn(sel.key, center);
        break;
      }
    }
  }

  private listEditHandles(sel: Selection): Array<{ handle: EditHandle; at: Point }> {
    const out: Array<{ handle: EditHandle; at: Point }> = [];
    const line = this.selectionLine(sel);
    if (line) {
      out.push({ handle: { sel, kind: "point", index: 0 }, at: line.p1 });
      out.push({ handle: { sel, kind: "point", index: 1 }, at: line.p2 });
      return out;
    }
    const arc = this.selectionArc(sel);
    if (arc) {
      out.push({ handle: { sel, kind: "point", index: 0 }, at: arc.p1 });
      out.push({ handle: { sel, kind: "point", index: 1 }, at: arc.p2 });
      out.push({ handle: { sel, kind: "point", index: 2 }, at: arc.p3 });
      return out;
    }
    if (sel.type === "collider") {
      const c = this.map.colliders[sel.index];
      if (c?.kind === "circle") {
        out.push({ handle: { sel, kind: "center" }, at: { x: c.x, y: c.y } });
        out.push({
          handle: { sel, kind: "radius" },
          at: { x: c.x + c.radius, y: c.y },
        });
        return out;
      }
    }
    if (sel.type === "shape") {
      const s = this.map.shapes[sel.index];
      if (s?.kind === "circle") {
        out.push({ handle: { sel, kind: "center" }, at: { x: s.x, y: s.y } });
        out.push({
          handle: { sel, kind: "radius" },
          at: { x: s.x + s.radius, y: s.y },
        });
        return out;
      }
    }
    if (sel.type === "spawn") {
      const p = this.spawnPoint(sel.key);
      if (p) out.push({ handle: { sel, kind: "center" }, at: p });
      return out;
    }
    const r = this.selectionRect(sel);
    if (r && this.rectHasCornerHandles(sel)) {
      const corners: Point[] = [
        { x: r.x, y: r.y },
        { x: r.x + r.w, y: r.y },
        { x: r.x + r.w, y: r.y + r.h },
        { x: r.x, y: r.y + r.h },
      ];
      corners.forEach((at, corner) => {
        out.push({
          handle: { sel, kind: "corner", corner: corner as 0 | 1 | 2 | 3 },
          at,
        });
      });
      if (this.supportsRoundRect(sel)) {
        out.push({
          handle: { sel, kind: "roundness" },
          at: roundnessHandleAt(r, this.selectionRectCornerRadius(sel)),
        });
      }
    }
    return out;
  }

  private rectHasCornerHandles(sel: Selection): boolean {
    if (sel.type === "visual") return true;
    if (sel.type === "shape") {
      return this.map.shapes[sel.index]?.kind === "rect";
    }
    if (sel.type === "collider") {
      return this.map.colliders[sel.index]?.kind === "rect";
    }
    if (sel.type === "goal_west") return this.map.goal_west?.kind === "rect";
    if (sel.type === "goal_east") return this.map.goal_east?.kind === "rect";
    return false;
  }

  private hitTestEditHandle(x: number, y: number, sel: Selection): EditHandle | null {
    const r = this.handleHitRadius();
    let best: EditHandle | null = null;
    let bestD = r;
    for (const { handle, at } of this.listEditHandles(sel)) {
      const d = dist(x, y, at.x, at.y);
      if (d <= bestD) {
        bestD = d;
        best = handle;
      }
    }
    return best;
  }

  private resizeRectCorner(rect: Rect, corner: 0 | 1 | 2 | 3, point: Point): Rect {
    const snapped = this.snapPoint(point);
    const px = snapped.x;
    const py = snapped.y;
    const x2 = rect.x + rect.w;
    const y2 = rect.y + rect.h;
    let x0 = rect.x;
    let y0 = rect.y;
    let x1 = x2;
    let y1 = y2;
    switch (corner) {
      case 0:
        x0 = px;
        y0 = py;
        break;
      case 1:
        x1 = px;
        y0 = py;
        break;
      case 2:
        x1 = px;
        y1 = py;
        break;
      case 3:
        x0 = px;
        y1 = py;
        break;
    }
    if (x0 > x1) [x0, x1] = [x1, x0];
    if (y0 > y1) [y0, y1] = [y1, y0];
    return this.snapRectToGrid({ x: x0, y: y0, w: x1 - x0, h: y1 - y0 });
  }

  private applyEditHandle(handle: EditHandle, pt: Point) {
    const { sel } = handle;
    if (handle.kind === "point") {
      const line = this.selectionLine(sel);
      if (line) {
        if (handle.index === 0) {
          this.applyLineMove(sel, { p1: pt, p2: line.p2 });
        } else {
          this.applyLineMove(sel, { p1: line.p1, p2: pt });
        }
        return;
      }
      const arc = this.selectionArc(sel);
      if (arc) {
        const next = { ...arc };
        if (handle.index === 0) next.p1 = pt;
        else if (handle.index === 1) next.p2 = pt;
        else next.p3 = pt;
        this.applyArcMove(sel, next);
        if (sel.type === "collider") this.applyLineArcTangentJoins();
      }
      return;
    }
    if (handle.kind === "center") {
      if (sel.type === "collider") {
        const c = this.map.colliders[sel.index];
        if (c?.kind === "circle") {
          c.x = pt.x;
          c.y = pt.y;
        }
      } else if (sel.type === "shape") {
        const s = this.map.shapes[sel.index];
        if (s?.kind === "circle") {
          s.x = pt.x;
          s.y = pt.y;
        }
      } else if (sel.type === "spawn") {
        this.setSpawn(sel.key, pt);
      }
      return;
    }
    if (handle.kind === "radius") {
      if (sel.type === "collider") {
        const c = this.map.colliders[sel.index];
        if (c?.kind === "circle") {
          const g = this.snapStep();
          c.radius = Math.max(g * 0.5, dist(pt.x, pt.y, c.x, c.y));
        }
      } else if (sel.type === "shape") {
        const s = this.map.shapes[sel.index];
        if (s?.kind === "circle") {
          const g = this.snapStep();
          s.radius = Math.max(g * 0.5, dist(pt.x, pt.y, s.x, s.y));
        }
      }
      return;
    }
    if (handle.kind === "corner") {
      const r = this.selectionRect(sel);
      if (!r) return;
      this.applyRect(sel, this.resizeRectCorner(r, handle.corner, pt));
      return;
    }
    if (handle.kind === "roundness") {
      const r = this.selectionRect(sel);
      if (!r) return;
      this.setSelectionRectCornerRadius(sel, pt.x - r.x);
    }
  }

  private snapSelectedGeometryToGrid(opts?: { skipRects?: boolean }) {
    for (const sel of this.selections) {
      const line = this.selectionLine(sel);
      if (line) {
        this.applyLineMove(sel, {
          p1: this.snapPoint(line.p1),
          p2: this.snapPoint(line.p2),
        });
        continue;
      }
      const arc = this.selectionArc(sel);
      if (arc) {
        this.applyArcMove(sel, {
          p1: this.snapPoint(arc.p1),
          p2: this.snapPoint(arc.p2),
          p3: this.snapPoint(arc.p3),
        });
        continue;
      }
      if (sel.type === "shape") {
        const s = this.map.shapes[sel.index];
        if (s?.kind === "circle") {
          const center = this.snapPoint({ x: s.x, y: s.y });
          const edge = this.snapPoint({ x: s.x + s.radius, y: s.y });
          const g = this.snapStep();
          s.x = center.x;
          s.y = center.y;
          s.radius = Math.max(g * 0.5, dist(center.x, center.y, edge.x, edge.y));
          continue;
        }
      }
      if (sel.type === "collider") {
        const c = this.map.colliders[sel.index];
        if (c?.kind === "circle") {
          const center = this.snapPoint({ x: c.x, y: c.y });
          const edge = this.snapPoint({ x: c.x + c.radius, y: c.y });
          const g = this.snapStep();
          c.x = center.x;
          c.y = center.y;
          c.radius = Math.max(g * 0.5, dist(center.x, center.y, edge.x, edge.y));
          continue;
        }
      }
      if (sel.type === "spawn") {
        const p = this.spawnPoint(sel.key);
        if (p) this.setSpawn(sel.key, this.snapPoint(p));
        continue;
      }
      const r = this.selectionRect(sel);
      if (r) {
        if (opts?.skipRects) continue;
        this.applyRect(sel, this.snapRectToGrid(r));
      }
    }
    this.applyLineArcTangentJoins();
  }

  private drawEditHandles(ctx: CanvasRenderingContext2D, sel: Selection) {
    const r = 7 / this.zoom;
    for (const { handle, at } of this.listEditHandles(sel)) {
      ctx.beginPath();
      ctx.arc(at.x, at.y, r, 0, Math.PI * 2);
      ctx.fillStyle =
        handle.kind === "radius"
          ? "rgba(255,209,102,0.95)"
          : handle.kind === "roundness"
            ? "rgba(255,140,255,0.95)"
            : "rgba(125,255,179,0.95)";
      ctx.fill();
      ctx.strokeStyle = "#0b0f16";
      ctx.lineWidth = 1.5 / this.zoom;
      ctx.stroke();
    }
  }

  private setStatus(text: string) {
    this.statusMessage = text;
  }

  private updateStatusBar() {
    const undo = this.history.canUndo() ? "undo" : "—";
    const redo = this.history.canRedo() ? "redo" : "—";
    this.statusEl.textContent = `${this.statusMessage} | zoom ${this.zoom.toFixed(2)} | cam (${this.camX.toFixed(0)}, ${this.camY.toFixed(0)}) | Ctrl+Z ${undo} | Ctrl+Shift+Z ${redo} | Shift+drag pan | wheel zoom`;
  }

  private loop() {
    requestAnimationFrame(() => this.loop());
    this.draw();
  }

  private draw() {
    const ctx = this.canvas.getContext("2d")!;
    ctx.setTransform(devicePixelRatio, 0, 0, devicePixelRatio, 0, 0);
    const viewW = this.viewSize.w;
    const viewH = this.viewSize.h;
    ctx.clearRect(0, 0, viewW, viewH);
    ctx.save();
    ctx.translate(viewW * 0.5, viewH * 0.5);
    ctx.scale(this.zoom, this.zoom);
    ctx.translate(-this.camX, -this.camY);

    this.drawBackground(ctx);
    this.drawGrid(ctx);
    for (const entry of buildDrawableOrder(this.map)) {
      if (entry.kind === "visual") {
        this.drawVisual(ctx, this.map.visuals[entry.index]);
      } else {
        this.drawColorShape(ctx, this.map.shapes[entry.index]);
      }
    }
    if (this.showLogic) this.drawContentTags(ctx);
    if (this.shouldDrawLogic()) this.drawLogic(ctx);
    this.drawShapePreview(ctx);
    this.drawHoverMarker(ctx);
    this.drawMarquee(ctx);
    for (const sel of this.selections) {
      this.drawSelectionHighlight(ctx, sel);
    }
    if (this.selections.length > 0 && !this.showLogic) {
      for (const sel of this.selections) {
        this.drawSelectionTag(ctx, sel, true);
      }
    }
    if (this.selections.length === 1) {
      const sole = this.selections[0];
      if (!isObjectLocked(this.map, sole)) {
        this.drawEditHandles(ctx, sole);
      }
    }

    ctx.restore();
    this.updateStatusBar();
  }

  private drawMarquee(ctx: CanvasRenderingContext2D) {
    if (!this.marqueeRect) return;
    const r = this.marqueeRect;
    ctx.fillStyle = "rgba(110,161,255,0.1)";
    ctx.strokeStyle = "rgba(110,161,255,0.95)";
    ctx.lineWidth = 1.5 / this.zoom;
    ctx.setLineDash([8 / this.zoom, 5 / this.zoom]);
    ctx.fillRect(r.x, r.y, r.w, r.h);
    ctx.strokeRect(r.x, r.y, r.w, r.h);
    ctx.setLineDash([]);
  }

  private drawSelectionHighlight(ctx: CanvasRenderingContext2D, sel: Selection) {
    const locked = isObjectLocked(this.map, sel);
    const stroke = locked ? "#ffd166" : "#6ea1ff";
    const fill = locked ? "rgba(255,209,102,0.12)" : "rgba(110,161,255,0.12)";
    const lineW = 2.5 / this.zoom;
    const line = this.selectionLine(sel);
    if (line) {
      ctx.fillStyle = fill;
      const b = lineBounds(line.p1, line.p2);
      ctx.fillRect(b.x, b.y, b.w, b.h);
      drawLineSegment(ctx, line.p1, line.p2, stroke, lineW);
      return;
    }
    const arc = this.selectionArc(sel);
    if (arc) {
      const b = boundsOfArc(arc);
      ctx.fillStyle = fill;
      ctx.fillRect(b.x, b.y, b.w, b.h);
      ctx.strokeStyle = stroke;
      ctx.lineWidth = lineW;
      if (locked) ctx.setLineDash([8 / this.zoom, 5 / this.zoom]);
      drawArcPath(ctx, arc);
      drawArcPoints(ctx, arc, this.zoom, stroke);
      if (locked) ctx.setLineDash([]);
      return;
    }
    const sr = this.selectionRect(sel);
    if (!sr) return;
    const cr = this.supportsRoundRect(sel) ? this.selectionRectCornerRadius(sel) : 0;
    if (locked) ctx.setLineDash([8 / this.zoom, 5 / this.zoom]);
    if (cr > 0 && traceRoundRectPath(ctx, sr, cr)) {
      ctx.fillStyle = fill;
      ctx.fill();
      ctx.strokeStyle = stroke;
      ctx.lineWidth = lineW;
      ctx.stroke();
      if (locked) ctx.setLineDash([]);
      return;
    }
    ctx.fillStyle = fill;
    ctx.fillRect(sr.x, sr.y, sr.w, sr.h);
    this.strokeRect(ctx, sr, stroke, 2.5);
    if (locked) ctx.setLineDash([]);
  }

  private gridExtents(): Rect {
    const pb = this.map.player_bounds;
    const pad = Math.max(pb.w, pb.h, 400);
    return {
      x: pb.x - pad * 0.15,
      y: pb.y - pad * 0.15,
      w: pb.w + pad * 0.3,
      h: pb.h + pad * 0.3,
    };
  }

  private drawShapePreview(ctx: CanvasRenderingContext2D) {
    const color = this.previewColor();
    const style = this.tool === "color_shape" ? this.activeDrawStyle() : null;
    if (this.previewRect) {
      const previewCorner =
        this.effectiveShape() === "square"
          ? clampCornerRadius(this.previewRect, this.cornerRadius)
          : 0;
      if (style) {
        this.paintStyledRect(ctx, this.previewRect, style, previewCorner);
      } else if (previewCorner > 0 && (this.tool === "collider" || this.tool === "color_shape")) {
        this.strokeRoundRect(ctx, this.previewRect, previewCorner, color, 2);
      } else {
        this.drawPreview(ctx, this.previewRect, color);
      }
    }
    if (this.previewCircle) {
      if (style) {
        this.paintStyledCircle(
          ctx,
          this.previewCircle.x,
          this.previewCircle.y,
          this.previewCircle.r,
          style,
        );
      } else {
        ctx.strokeStyle = color;
        ctx.lineWidth = 2 / this.zoom;
        ctx.beginPath();
        ctx.arc(
          this.previewCircle.x,
          this.previewCircle.y,
          this.previewCircle.r,
          0,
          Math.PI * 2,
        );
        ctx.stroke();
      }
    }
    if (this.previewLine) {
      const lineColor = style?.stroke ?? color;
      const lineWidth = (style?.strokeWidth ?? 2) / this.zoom;
      drawLineSegment(ctx, this.previewLine.p1, this.previewLine.p2, lineColor, lineWidth);
    }

    if (this.clickPoints.length === 0) return;
    ctx.strokeStyle = color;
    ctx.lineWidth = 2 / this.zoom;
    for (const p of this.clickPoints) {
      ctx.fillStyle = color;
      ctx.beginPath();
      ctx.arc(p.x, p.y, 6 / this.zoom, 0, Math.PI * 2);
      ctx.fill();
    }
    if (this.clickPoints.length >= 2) {
      ctx.beginPath();
      ctx.moveTo(this.clickPoints[0].x, this.clickPoints[0].y);
      ctx.lineTo(this.clickPoints[1].x, this.clickPoints[1].y);
      ctx.stroke();
    }
    const shape = this.effectiveShape();
    if (shape === "arc" && this.clickPoints.length === 2 && this.hoverSnapped) {
      drawArcPath(ctx, {
        p1: this.clickPoints[0],
        p2: this.clickPoints[1],
        p3: this.hoverSnapped,
      });
    }
    if (shape === "arc" && this.clickPoints.length === 3) {
      drawArcPath(ctx, {
        p1: this.clickPoints[0],
        p2: this.clickPoints[1],
        p3: this.clickPoints[2],
      });
    }
  }

  private drawHoverMarker(ctx: CanvasRenderingContext2D) {
    if (!this.pointerInside || !this.hoverCell || !this.hoverSnapped) return;

    const step = this.snapStep();
    const cellRect = gridCellRect(this.hoverCell.col, this.hoverCell.row, step);
    const p = this.hoverSnapped;

    ctx.fillStyle = "rgba(110,161,255,0.08)";
    ctx.fillRect(cellRect.x, cellRect.y, cellRect.w, cellRect.h);

    ctx.strokeStyle = "rgba(255,209,102,0.55)";
    ctx.lineWidth = 1.5 / this.zoom;
    ctx.strokeRect(cellRect.x, cellRect.y, cellRect.w, cellRect.h);

    const cross = step * 0.35;
    ctx.strokeStyle = "rgba(255,209,102,0.9)";
    ctx.lineWidth = 1.25 / this.zoom;
    ctx.beginPath();
    ctx.moveTo(p.x - cross, p.y);
    ctx.lineTo(p.x + cross, p.y);
    ctx.moveTo(p.x, p.y - cross);
    ctx.lineTo(p.x, p.y + cross);
    ctx.stroke();

    ctx.fillStyle = "rgba(255,209,102,0.95)";
    ctx.beginPath();
    ctx.arc(p.x, p.y, 4 / this.zoom, 0, Math.PI * 2);
    ctx.fill();

    if (this.isDrawTool() && this.isClickShape()) {
      const needed = this.effectiveShape() === "arc" ? 3 : 2;
      ctx.fillStyle = "rgba(255,255,255,0.85)";
      ctx.font = `${12 / this.zoom}px sans-serif`;
      ctx.fillText(
        `${this.clickPoints.length + 1}/${needed}`,
        p.x + 8 / this.zoom,
        p.y - 8 / this.zoom,
      );
    }
  }

  private drawGoalZone(
    ctx: CanvasRenderingContext2D,
    zone: GoalZone | null,
    color: string,
    label: string,
  ) {
    if (!zone) return;
    ctx.strokeStyle = color;
    ctx.lineWidth = 2 / this.zoom;
    if (zone.kind === "rect") {
      ctx.fillStyle =
        color === westGoalColor() ? "rgba(77,171,255,0.14)" : "rgba(255,107,107,0.14)";
      ctx.fillRect(zone.rect.x, zone.rect.y, zone.rect.w, zone.rect.h);
      this.strokeRect(ctx, zone.rect, color, 2);
    } else {
      ctx.fillStyle =
        color === westGoalColor() ? "rgba(77,171,255,0.1)" : "rgba(255,107,107,0.1)";
      drawArcPath(ctx, zone.arc);
      ctx.fill();
      drawArcPath(ctx, zone.arc);
      drawArcPoints(ctx, zone.arc, this.zoom, color);
    }
    const bounds = this.goalZoneBounds(zone);
    if (bounds) {
      this.drawCanvasLabelAtRectCenter(ctx, bounds, label, color, true);
    }
  }

  private drawBackground(ctx: CanvasRenderingContext2D) {
    const bg = this.map.background;
    if (!bg) return;
    const dest = effectiveBackgroundDest(this.map);
    if (bg.color) {
      ctx.fillStyle = bg.color;
      ctx.fillRect(dest.x, dest.y, dest.w, dest.h);
    }
    if (this.backgroundImage && bg.image) {
      const src = bg.src ?? {
        x: 0,
        y: 0,
        w: this.backgroundImage.naturalWidth || this.backgroundImage.width,
        h: this.backgroundImage.naturalHeight || this.backgroundImage.height,
      };
      this.drawTexturedRegion(ctx, this.backgroundImage, src, dest, false, bg.repeat === true);
    } else if (bg.image && !this.backgroundImage) {
      ctx.fillStyle = "rgba(255,255,255,0.06)";
      ctx.fillRect(dest.x, dest.y, dest.w, dest.h);
      ctx.strokeStyle = "rgba(255,209,102,0.55)";
      ctx.lineWidth = 1.5 / this.zoom;
      ctx.strokeRect(dest.x, dest.y, dest.w, dest.h);
      this.drawCanvasLabelAtRectCenter(
        ctx,
        dest,
        `Missing: ${bg.image}`,
        "#ffd166",
        false,
      );
    }
  }

  private drawGrid(ctx: CanvasRenderingContext2D) {
    if (!this.showGrid) return;
    const area = this.gridExtents();
    const g = this.gridSize;
    const major = g * 8;
    const x0 = Math.floor(area.x / g) * g;
    const y0 = Math.floor(area.y / g) * g;
    const x1 = area.x + area.w;
    const y1 = area.y + area.h;
    const lw = 1 / this.zoom;

    ctx.lineWidth = lw;
    ctx.strokeStyle = "rgba(255,255,255,0.07)";
    for (let x = x0; x <= x1; x += g) {
      if (x % major === 0) continue;
      ctx.beginPath();
      ctx.moveTo(x, area.y);
      ctx.lineTo(x, area.y + area.h);
      ctx.stroke();
    }
    for (let y = y0; y <= y1; y += g) {
      if (y % major === 0) continue;
      ctx.beginPath();
      ctx.moveTo(area.x, y);
      ctx.lineTo(area.x + area.w, y);
      ctx.stroke();
    }

    ctx.strokeStyle = "rgba(110,161,255,0.28)";
    ctx.lineWidth = lw * 1.5;
    for (let x = Math.floor(x0 / major) * major; x <= x1; x += major) {
      ctx.beginPath();
      ctx.moveTo(x, area.y);
      ctx.lineTo(x, area.y + area.h);
      ctx.stroke();
    }
    for (let y = Math.floor(y0 / major) * major; y <= y1; y += major) {
      ctx.beginPath();
      ctx.moveTo(area.x, y);
      ctx.lineTo(area.x + area.w, y);
      ctx.stroke();
    }
  }

  private drawPreview(
    ctx: CanvasRenderingContext2D,
    r: Rect,
    color: string = this.previewColor(),
  ) {
    if (this.isVisualTool(this.tool)) {
      const sheet = toolSheet(this.tool);
      const img = sheet ? this.assets[sheet] : null;
      const src = this.srcForTool(this.tool);
      if (img && src) {
        this.drawTexturedRegion(ctx, img, src, r, false, this.tool === "ice_tile");
      }
    }
    this.strokeRect(ctx, r, color, 2);
  }

  private drawTexturedRegion(
    ctx: CanvasRenderingContext2D,
    img: HTMLImageElement,
    src: Rect,
    dest: Rect,
    flipX: boolean,
    tile: boolean,
  ) {
    if (tile) {
      for (let y = dest.y; y < dest.y + dest.h; y += src.h) {
        const dh = Math.min(src.h, dest.y + dest.h - y);
        for (let x = dest.x; x < dest.x + dest.w; x += src.w) {
          const dw = Math.min(src.w, dest.x + dest.w - x);
          this.blit(ctx, img, src, { x, y, w: dw, h: dh }, flipX);
        }
      }
      return;
    }
    this.blit(ctx, img, src, dest, flipX);
  }

  private blit(
    ctx: CanvasRenderingContext2D,
    img: HTMLImageElement,
    src: Rect,
    dest: Rect,
    flipX: boolean,
  ) {
    if (flipX) {
      ctx.save();
      ctx.translate(dest.x + dest.w, dest.y);
      ctx.scale(-1, 1);
      ctx.drawImage(img, src.x, src.y, src.w, src.h, 0, 0, dest.w, dest.h);
      ctx.restore();
    } else {
      ctx.drawImage(
        img,
        src.x,
        src.y,
        src.w,
        src.h,
        dest.x,
        dest.y,
        dest.w,
        dest.h,
      );
    }
  }

  private drawVisual(ctx: CanvasRenderingContext2D, piece: VisualPiece) {
    const sheet = visualKindSheet(piece.kind);
    const img = this.assets[sheet];
    const r = piece.dest;
    if (img) {
      const src =
        piece.use_src && piece.src
          ? piece.src
          : { x: 0, y: 0, w: img.width, h: img.height };
      const tile = piece.kind === "ice_tile" && !!piece.use_src && !!piece.src;
      this.drawTexturedRegion(ctx, img, src, r, !!piece.flip_x, tile);
    } else {
      ctx.fillStyle = "#667085";
      ctx.fillRect(r.x, r.y, r.w, r.h);
    }
  }

  private paintStyledRect(
    ctx: CanvasRenderingContext2D,
    r: Rect,
    style: ShapeStyle,
    cornerRadius = 0,
  ) {
    const cr = clampCornerRadius(r, cornerRadius);
    if (cr > 0) {
      paintRoundRect(ctx, r, cr, {
        fill: style.fill,
        stroke: style.stroke,
        lineWidth: (style.strokeWidth ?? 2) / this.zoom,
      });
      return;
    }
    if (style.fill) {
      ctx.fillStyle = style.fill;
      ctx.fillRect(r.x, r.y, r.w, r.h);
    }
    if (style.stroke) {
      this.strokeRect(ctx, r, style.stroke, style.strokeWidth ?? 2);
    }
  }

  private paintStyledCircle(
    ctx: CanvasRenderingContext2D,
    x: number,
    y: number,
    radius: number,
    style: ShapeStyle,
  ) {
    ctx.beginPath();
    ctx.arc(x, y, radius, 0, Math.PI * 2);
    if (style.fill) {
      ctx.fillStyle = style.fill;
      ctx.fill();
    }
    if (style.stroke) {
      ctx.strokeStyle = style.stroke;
      ctx.lineWidth = (style.strokeWidth ?? 2) / this.zoom;
      ctx.stroke();
    }
  }

  private drawColorShape(ctx: CanvasRenderingContext2D, shape: MapShape) {
    const style: ShapeStyle = {
      fill: shape.fill,
      stroke: shape.stroke,
      strokeWidth: shape.strokeWidth,
    };
    switch (shape.kind) {
      case "rect":
        this.paintStyledRect(ctx, shape.rect, style, shape.cornerRadius ?? 0);
        break;
      case "circle":
        this.paintStyledCircle(ctx, shape.x, shape.y, shape.radius, style);
        break;
      case "line":
        if (shape.stroke) {
          drawLineSegment(
            ctx,
            shape.p1,
            shape.p2,
            shape.stroke,
            (shape.strokeWidth ?? 2) / this.zoom,
          );
        }
        break;
      case "arc":
        paintArcPath(ctx, shape.arc, {
          fill: shape.fill,
          stroke: shape.stroke,
          lineWidth: (shape.strokeWidth ?? 2) / this.zoom,
        });
        break;
    }
  }

  private drawLogic(ctx: CanvasRenderingContext2D) {
    this.drawGoalZone(
      ctx,
      this.map.goal_west,
      westGoalColor(),
      goalDisplayTag(this.map.goal_west, "west"),
    );
    this.drawGoalZone(
      ctx,
      this.map.goal_east,
      eastGoalColor(),
      goalDisplayTag(this.map.goal_east, "east"),
    );
    const pb = this.map.player_bounds;
    ctx.fillStyle = "rgba(125,255,179,0.04)";
    ctx.fillRect(pb.x, pb.y, pb.w, pb.h);
    this.strokeRect(ctx, pb, "#7dffb3", 1.25);
    this.drawCameraPreviews(ctx);

    for (const c of this.map.colliders) {
      if (c.kind === "rect") {
        const cr = c.cornerRadius ?? 0;
        if (cr > 0) this.strokeRoundRect(ctx, c.rect, cr, "#ff9f43", 2);
        else this.strokeRect(ctx, c.rect, "#ff9f43", 2);
        this.drawCanvasLabelAtRectCenter(ctx, c.rect, colliderDisplayTag(c), "#ff9f43");
      } else if (c.kind === "arc") {
        ctx.strokeStyle = "#ff9f43";
        ctx.lineWidth = 2 / this.zoom;
        drawArcPath(ctx, c.arc);
        const bounds = boundsOfArc(c.arc);
        this.drawCanvasLabelAtRectCenter(ctx, bounds, colliderDisplayTag(c), "#ff9f43");
      } else if (c.kind === "line") {
        drawLineSegment(ctx, c.p1, c.p2, "#ff9f43", 2 / this.zoom);
        const bounds = lineBounds(c.p1, c.p2);
        this.drawCanvasLabelAtRectCenter(ctx, bounds, colliderDisplayTag(c), "#ff9f43");
      } else {
        ctx.strokeStyle = "#ff9f43";
        ctx.beginPath();
        ctx.arc(c.x, c.y, c.radius, 0, Math.PI * 2);
        ctx.stroke();
        this.drawCanvasLabelAtRectCenter(
          ctx,
          { x: c.x - c.radius, y: c.y - c.radius, w: c.radius * 2, h: c.radius * 2 },
          colliderDisplayTag(c),
          "#ff9f43",
        );
      }
    }

    const sp = this.map.spawns;
    if (sp.puck_start) {
      this.drawPoint(ctx, sp.puck_start, "#ffd166", 8);
      this.drawCanvasLabel(ctx, sp.puck_start.x, sp.puck_start.y, "Puck start", "#ffd166");
    }
    if (sp.puck_faceoff) {
      this.drawPoint(ctx, sp.puck_faceoff, "#ffd166", 6);
      this.drawCanvasLabel(ctx, sp.puck_faceoff.x, sp.puck_faceoff.y, "Puck faceoff", "#ffd166");
    }
    if (sp.team_a.goalie) {
      this.drawPoint(ctx, sp.team_a.goalie, westGoalColor(), 12);
      this.drawCanvasLabel(
        ctx,
        sp.team_a.goalie.x,
        sp.team_a.goalie.y,
        spawnKeyLabel("team_a_goalie"),
        westGoalColor(),
      );
    }
    for (const [i, sk] of sp.team_a.skaters.entries()) {
      if (sk) {
        this.drawPoint(ctx, sk, westGoalColor(), 8);
        this.drawCanvasLabel(
          ctx,
          sk.x,
          sk.y,
          spawnKeyLabel(`team_a_skater_${i}` as SpawnKey),
          westGoalColor(),
        );
      }
    }
    if (sp.team_b.goalie) {
      this.drawPoint(ctx, sp.team_b.goalie, eastGoalColor(), 12);
      this.drawCanvasLabel(
        ctx,
        sp.team_b.goalie.x,
        sp.team_b.goalie.y,
        spawnKeyLabel("team_b_goalie"),
        eastGoalColor(),
      );
    }
    for (const [i, sk] of sp.team_b.skaters.entries()) {
      if (sk) {
        this.drawPoint(ctx, sk, eastGoalColor(), 8);
        this.drawCanvasLabel(
          ctx,
          sk.x,
          sk.y,
          spawnKeyLabel(`team_b_skater_${i}` as SpawnKey),
          eastGoalColor(),
        );
      }
    }
  }

  private drawCameraPreviews(ctx: CanvasRenderingContext2D) {
    const play = this.map.player_bounds;
    const cx = this.camX;
    const cy = this.camY;
    for (const level of CAMERA_PREVIEW_LEVELS) {
      const view = worldViewRectAtFraction(cx, cy, level.frac, play);
      ctx.fillStyle = level.dashed ? "rgba(183,148,246,0.04)" : "rgba(183,148,246,0.07)";
      ctx.fillRect(view.x, view.y, view.w, view.h);
      if (level.dashed) {
        this.strokeDashedRect(ctx, view, "#b794f6", 1.5, [10 / this.zoom, 7 / this.zoom]);
      } else {
        this.strokeRect(ctx, view, "#b794f6", 2);
      }
    }
  }

  private strokeDashedRect(
    ctx: CanvasRenderingContext2D,
    r: Rect,
    color: string,
    width: number,
    dash: number[],
  ) {
    ctx.strokeStyle = color;
    ctx.lineWidth = width / this.zoom;
    ctx.setLineDash(dash);
    ctx.strokeRect(r.x, r.y, r.w, r.h);
    ctx.setLineDash([]);
  }

  private strokeRoundRect(
    ctx: CanvasRenderingContext2D,
    r: Rect,
    cornerRadius: number,
    color: string,
    width: number,
  ) {
    if (!traceRoundRectPath(ctx, r, cornerRadius)) return;
    ctx.strokeStyle = color;
    ctx.lineWidth = width / this.zoom;
    ctx.stroke();
  }

  private strokeRect(
    ctx: CanvasRenderingContext2D,
    r: Rect,
    color: string,
    width: number,
  ) {
    ctx.strokeStyle = color;
    ctx.lineWidth = width / this.zoom;
    ctx.strokeRect(r.x, r.y, r.w, r.h);
  }

  private drawPoint(
    ctx: CanvasRenderingContext2D,
    p: Point,
    color: string,
    radius: number,
  ) {
    ctx.fillStyle = color;
    ctx.beginPath();
    ctx.arc(p.x, p.y, radius / this.zoom, 0, Math.PI * 2);
    ctx.fill();
  }

  private drawContentTags(ctx: CanvasRenderingContext2D) {
    for (const entry of buildDrawableOrder(this.map)) {
      if (entry.kind === "visual") {
        const piece = this.map.visuals[entry.index];
        if (!piece) continue;
        this.drawCanvasLabelAtRectCenter(
          ctx,
          piece.dest,
          visualDisplayTag(piece),
          "#cbd5e1",
        );
      } else {
        const shape = this.map.shapes[entry.index];
        if (!shape) continue;
        const bounds = this.shapeBounds(shape);
        if (!bounds) continue;
        this.drawCanvasLabelAtRectCenter(ctx, bounds, shapeDisplayTag(shape), "#a5d8ff");
      }
    }
  }

  private shapeBounds(shape: MapShape): Rect | null {
    switch (shape.kind) {
      case "rect":
        return shape.rect;
      case "circle":
        return {
          x: shape.x - shape.radius,
          y: shape.y - shape.radius,
          w: shape.radius * 2,
          h: shape.radius * 2,
        };
      case "line":
        return lineBounds(shape.p1, shape.p2);
      case "arc":
        return boundsOfArc(shape.arc);
    }
  }

  private drawSelectionTag(ctx: CanvasRenderingContext2D, sel: Selection, emphasized: boolean) {
    if (sel.type === "spawn") {
      const p = this.spawnPoint(sel.key);
      if (!p) return;
      this.drawCanvasLabel(
        ctx,
        p.x,
        p.y,
        objectTag(this.map, sel),
        spawnTagColor(sel.key),
        emphasized,
      );
      return;
    }
    const bounds = this.selectionRect(sel);
    if (!bounds) return;
    this.drawCanvasLabelAtRectCenter(
      ctx,
      bounds,
      objectTag(this.map, sel),
      this.tagColorForSelection(sel),
      emphasized,
    );
  }

  private tagColorForSelection(sel: Selection): string {
    if (sel.type === "goal_west") return westGoalColor();
    if (sel.type === "goal_east") return eastGoalColor();
    if (sel.type === "collider") return "#ff9f43";
    if (sel.type === "shape") return "#a5d8ff";
    return "#cbd5e1";
  }

  private drawCanvasLabelAtRectCenter(
    ctx: CanvasRenderingContext2D,
    r: Rect,
    text: string,
    color: string,
    emphasized = false,
  ) {
    this.drawCanvasLabel(ctx, r.x + r.w * 0.5, r.y + r.h * 0.5, text, color, emphasized, true);
  }

  private drawCanvasLabel(
    ctx: CanvasRenderingContext2D,
    x: number,
    y: number,
    text: string,
    color: string,
    emphasized = false,
    centered = false,
  ) {
    const fontPx = Math.max(10, (emphasized ? 13 : 11) / this.zoom);
    ctx.save();
    ctx.font = `${fontPx}px sans-serif`;
    if (centered) {
      ctx.textAlign = "center";
      ctx.textBaseline = "middle";
    } else {
      ctx.textAlign = "left";
      ctx.textBaseline = "bottom";
    }
    const metrics = ctx.measureText(text);
    const pad = 4 / this.zoom;
    const w = metrics.width + pad * 2;
    const h = fontPx + pad * 1.5;
    const bx = centered ? x - w * 0.5 : x - pad;
    const by = centered ? y - h * 0.5 : y - h;
    ctx.fillStyle = emphasized ? "rgba(11,15,22,0.9)" : "rgba(11,15,22,0.78)";
    ctx.fillRect(bx, by, w, h);
    ctx.fillStyle = color;
    ctx.fillText(text, centered ? x : x, centered ? y : y - pad * 0.5);
    ctx.restore();
  }
}

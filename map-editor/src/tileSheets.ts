import type { Rect, Tool, VisualKind } from "./types";

export type SheetKey = "ice" | "walls" | "circle" | "goal";

export const SHEET_KEYS: SheetKey[] = ["ice", "walls", "circle", "goal"];

export const DEFAULT_TILE_CELL: Record<SheetKey, number> = {
  ice: 64,
  walls: 64,
  circle: 64,
  goal: 64,
};

export function visualKindSheet(kind: VisualKind): SheetKey {
  switch (kind) {
    case "ice_tile":
      return "ice";
    case "wall_frame":
      return "walls";
    case "circle_mark":
      return "circle";
    case "goal_sprite":
      return "goal";
  }
}

export function toolSheet(tool: Tool): SheetKey | null {
  switch (tool) {
    case "ice_tile":
      return "ice";
    case "wall_frame":
      return "walls";
    case "circle_mark":
      return "circle";
    case "goal_sprite":
      return "goal";
    default:
      return null;
  }
}

export function cellRect(col: number, row: number, cell: number): Rect {
  return { x: col * cell, y: row * cell, w: cell, h: cell };
}

export function pickCell(
  imgW: number,
  imgH: number,
  px: number,
  py: number,
  cell: number,
): Rect {
  const col = Math.max(0, Math.min(Math.floor(px / cell), Math.floor((imgW - 1) / cell)));
  const row = Math.max(0, Math.min(Math.floor(py / cell), Math.floor((imgH - 1) / cell)));
  const w = Math.min(cell, imgW - col * cell);
  const h = Math.min(cell, imgH - row * cell);
  return { x: col * cell, y: row * cell, w, h };
}

/** Pick the sheet cell whose area contains the pointer. */
export function pickCellContaining(
  imgW: number,
  imgH: number,
  px: number,
  py: number,
  cell: number,
): Rect {
  return pickCell(imgW, imgH, px, py, cell);
}

export function pickRegion(
  imgW: number,
  imgH: number,
  x0: number,
  y0: number,
  x1: number,
  y1: number,
  cell: number,
  snapToGrid: boolean,
): Rect {
  let left = Math.max(0, Math.min(x0, x1));
  let top = Math.max(0, Math.min(y0, y1));
  let right = Math.min(imgW, Math.max(x0, x1));
  let bottom = Math.min(imgH, Math.max(y0, y1));

  if (snapToGrid && cell > 0) {
    left = Math.floor(left / cell) * cell;
    top = Math.floor(top / cell) * cell;
    right = Math.min(imgW, Math.ceil(right / cell) * cell);
    bottom = Math.min(imgH, Math.ceil(bottom / cell) * cell);
  }

  if (right - left < 1) right = Math.min(imgW, left + (snapToGrid ? cell : 1));
  if (bottom - top < 1) bottom = Math.min(imgH, top + (snapToGrid ? cell : 1));

  return {
    x: left,
    y: top,
    w: Math.max(1, right - left),
    h: Math.max(1, bottom - top),
  };
}

export function clampSrcRect(imgW: number, imgH: number, src: Rect): Rect {
  const x = Math.max(0, Math.min(src.x, imgW - 1));
  const y = Math.max(0, Math.min(src.y, imgH - 1));
  const w = Math.max(1, Math.min(src.w, imgW - x));
  const h = Math.max(1, Math.min(src.h, imgH - y));
  return { x, y, w, h };
}

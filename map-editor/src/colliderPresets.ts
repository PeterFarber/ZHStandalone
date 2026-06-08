import type { Arc3, Collider, Point, Rect } from "./types";

export type IceBounds = { min_x: number; max_x: number; min_y: number; max_y: number };

export function iceBoundsFromRect(r: Rect): IceBounds {
  return { min_x: r.x, max_x: r.x + r.w, min_y: r.y, max_y: r.y + r.h };
}

/** Capsule rink boards as top/bottom rects + end arcs (matches legacy playfield collision). */
export function capsuleBoardColliders(bounds: IceBounds, wall = 20): Collider[] {
  const r = (bounds.max_y - bounds.min_y) * 0.5;
  const cy = (bounds.min_y + bounds.max_y) * 0.5;
  const cxL = bounds.min_x + r;
  const cxR = bounds.max_x - r;
  const flatW = Math.max(0, cxR - cxL);

  const leftArc: Arc3 = {
    p1: { x: cxL, y: bounds.min_y },
    p2: { x: bounds.min_x, y: cy },
    p3: { x: cxL, y: bounds.max_y },
  };
  const rightArc: Arc3 = {
    p1: { x: cxR, y: bounds.min_y },
    p2: { x: bounds.max_x, y: cy },
    p3: { x: cxR, y: bounds.max_y },
  };

  return [
    { kind: "rect", rect: { x: cxL, y: bounds.min_y - wall * 0.5, w: flatW, h: wall } },
    { kind: "rect", rect: { x: cxL, y: bounds.max_y - wall * 0.5, w: flatW, h: wall } },
    { kind: "arc", arc: leftArc },
    { kind: "arc", arc: rightArc },
  ];
}

export function migratePlayfieldToColliders(
  playfield: IceBounds | null | undefined,
  colliders: Collider[],
): Collider[] {
  if (!playfield || colliders.length > 0) return colliders;
  return capsuleBoardColliders(playfield);
}

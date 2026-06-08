import type { Point } from "./types";

export function distToSegment(
  px: number,
  py: number,
  x1: number,
  y1: number,
  x2: number,
  y2: number,
): number {
  const dx = x2 - x1;
  const dy = y2 - y1;
  const lenSq = dx * dx + dy * dy;
  if (lenSq < 1e-8) return Math.hypot(px - x1, py - y1);
  const t = Math.max(0, Math.min(1, ((px - x1) * dx + (py - y1) * dy) / lenSq));
  return Math.hypot(px - (x1 + t * dx), py - (y1 + t * dy));
}

export function hitTestLine(
  px: number,
  py: number,
  p1: Point,
  p2: Point,
  threshold: number,
): boolean {
  return distToSegment(px, py, p1.x, p1.y, p2.x, p2.y) <= threshold;
}

export function drawLineSegment(
  ctx: CanvasRenderingContext2D,
  p1: Point,
  p2: Point,
  color: string,
  width: number,
) {
  ctx.strokeStyle = color;
  ctx.lineWidth = width;
  ctx.beginPath();
  ctx.moveTo(p1.x, p1.y);
  ctx.lineTo(p2.x, p2.y);
  ctx.stroke();
}

export function lineBounds(p1: Point, p2: Point): { x: number; y: number; w: number; h: number } {
  const minX = Math.min(p1.x, p2.x);
  const minY = Math.min(p1.y, p2.y);
  return { x: minX, y: minY, w: Math.abs(p2.x - p1.x), h: Math.abs(p2.y - p1.y) };
}

import type { Point, Rect } from "./types";

export function clampCornerRadius(rect: Rect, radius: number): number {
  if (!Number.isFinite(radius) || radius <= 0) return 0;
  return Math.min(radius, rect.w * 0.5, rect.h * 0.5);
}

export function traceRoundRectPath(
  ctx: CanvasRenderingContext2D,
  rect: Rect,
  radius: number,
): boolean {
  const r = clampCornerRadius(rect, radius);
  const { x, y, w, h } = rect;
  if (w <= 0 || h <= 0) return false;
  ctx.beginPath();
  if (r <= 0) {
    ctx.rect(x, y, w, h);
    return true;
  }
  if (typeof ctx.roundRect === "function") {
    ctx.roundRect(x, y, w, h, r);
    return true;
  }
  ctx.moveTo(x + r, y);
  ctx.lineTo(x + w - r, y);
  ctx.arcTo(x + w, y, x + w, y + r, r);
  ctx.lineTo(x + w, y + h - r);
  ctx.arcTo(x + w, y + h, x + w - r, y + h, r);
  ctx.lineTo(x + r, y + h);
  ctx.arcTo(x, y + h, x, y + h - r, r);
  ctx.lineTo(x, y + r);
  ctx.arcTo(x, y, x + r, y, r);
  ctx.closePath();
  return true;
}

export function paintRoundRect(
  ctx: CanvasRenderingContext2D,
  rect: Rect,
  radius: number,
  opts: { fill?: string; stroke?: string; lineWidth?: number },
) {
  if (!traceRoundRectPath(ctx, rect, radius)) return;
  if (opts.fill) {
    ctx.fillStyle = opts.fill;
    ctx.fill();
  }
  if (opts.stroke) {
    ctx.strokeStyle = opts.stroke;
    ctx.lineWidth = opts.lineWidth ?? 1;
    ctx.stroke();
  }
}

/** Point inside a filled rounded rectangle. */
export function pointInRoundRect(px: number, py: number, rect: Rect, radius: number): boolean {
  const r = clampCornerRadius(rect, radius);
  const cx = rect.x + rect.w * 0.5;
  const cy = rect.y + rect.h * 0.5;
  const hx = rect.w * 0.5 - r;
  const hy = rect.h * 0.5 - r;
  const ax = Math.abs(px - cx);
  const ay = Math.abs(py - cy);
  if (ax <= hx) return ay <= rect.h * 0.5;
  if (ay <= hy) return ax <= rect.w * 0.5;
  const dx = ax - hx;
  const dy = ay - hy;
  return dx * dx + dy * dy <= r * r;
}

export function roundnessHandleAt(rect: Rect, cornerRadius: number): Point {
  const r = clampCornerRadius(rect, cornerRadius);
  return { x: rect.x + r, y: rect.y + Math.min(rect.h * 0.5, Math.max(8, r * 0.5)) };
}

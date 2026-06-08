import type { Arc3, Point } from "./types";

export type ArcGeom = {
  cx: number;
  cy: number;
  r: number;
  a1: number;
  a2: number;
  ccw: boolean;
  p1: Point;
  p2: Point;
  p3: Point;
};

const TAU = Math.PI * 2;

function normAngle(a: number): number {
  a %= TAU;
  if (a < 0) a += TAU;
  return a;
}

function angleBetweenCCW(a0: number, aMid: number, a1: number): boolean {
  const d0 = normAngle(aMid - a0);
  const d1 = normAngle(a1 - a0);
  return d1 >= d0;
}

export function arcFrom3Points(arc: Arc3): ArcGeom | null {
  const { p1, p2, p3 } = arc;
  const d =
    2 *
    (p1.x * (p2.y - p3.y) + p2.x * (p3.y - p1.y) + p3.x * (p1.y - p2.y));
  if (Math.abs(d) < 1e-4) return null;

  const p1sq = p1.x * p1.x + p1.y * p1.y;
  const p2sq = p2.x * p2.x + p2.y * p2.y;
  const p3sq = p3.x * p3.x + p3.y * p3.y;
  const cx = (p1sq * (p2.y - p3.y) + p2sq * (p3.y - p1.y) + p3sq * (p1.y - p2.y)) / d;
  const cy = (p1sq * (p3.x - p2.x) + p2sq * (p1.x - p3.x) + p3sq * (p2.x - p1.x)) / d;
  const r = Math.hypot(p1.x - cx, p1.y - cy);
  if (r < 1e-3) return null;

  const a1 = Math.atan2(p1.y - cy, p1.x - cx);
  const a2 = Math.atan2(p2.y - cy, p2.x - cx);
  const a3 = Math.atan2(p3.y - cy, p3.x - cx);
  const ccw = angleBetweenCCW(a1, a3, a2);
  return { cx, cy, r, a1, a2, ccw, p1, p2, p3 };
}

function clampToArc(angle: number, g: ArcGeom): number {
  const a = normAngle(angle);
  const start = normAngle(g.a1);
  const end = normAngle(g.a2);
  if (g.ccw) {
    const rel = normAngle(a - start);
    const span = normAngle(end - start);
    if (rel <= span) return a;
    const ds = normAngle(start - a);
    const de = normAngle(end - a);
    return ds <= de ? start : end;
  }
  const rel = normAngle(start - a);
  const span = normAngle(start - end);
  if (rel <= span) return a;
  const ds = normAngle(start - a);
  const de = normAngle(end - a);
  return ds <= de ? start : end;
}

export function closestPointOnArc(px: number, py: number, g: ArcGeom): Point {
  const ang = clampToArc(Math.atan2(py - g.cy, px - g.cx), g);
  const onArc = { x: g.cx + g.r * Math.cos(ang), y: g.cy + g.r * Math.sin(ang) };
  const candidates = [onArc, g.p1, g.p2];
  let best = candidates[0];
  let bestD = Math.hypot(px - best.x, py - best.y);
  for (let i = 1; i < candidates.length; i++) {
    const d = Math.hypot(px - candidates[i].x, py - candidates[i].y);
    if (d < bestD) {
      bestD = d;
      best = candidates[i];
    }
  }
  return best;
}

export function hitTestArc(
  px: number,
  py: number,
  arc: Arc3,
  threshold: number,
): boolean {
  const g = arcFrom3Points(arc);
  if (!g) return false;
  const c = closestPointOnArc(px, py, g);
  return Math.hypot(px - c.x, py - c.y) <= threshold;
}

export function boundsOfArc(arc: Arc3): { x: number; y: number; w: number; h: number } {
  const pts = [arc.p1, arc.p2, arc.p3];
  const g = arcFrom3Points(arc);
  if (g) {
    const steps = 16;
    const start = g.a1;
    const end = g.a2;
    for (let i = 0; i <= steps; i++) {
      const t = i / steps;
      const ang = g.ccw
        ? start + t * normAngle(end - start)
        : start - t * normAngle(start - end);
      pts.push({ x: g.cx + g.r * Math.cos(ang), y: g.cy + g.r * Math.sin(ang) });
    }
  }
  let minX = pts[0].x;
  let maxX = pts[0].x;
  let minY = pts[0].y;
  let maxY = pts[0].y;
  for (const p of pts) {
    minX = Math.min(minX, p.x);
    maxX = Math.max(maxX, p.x);
    minY = Math.min(minY, p.y);
    maxY = Math.max(maxY, p.y);
  }
  return { x: minX, y: minY, w: maxX - minX, h: maxY - minY };
}

export function drawArcPath(ctx: CanvasRenderingContext2D, arc: Arc3): boolean {
  const g = arcFrom3Points(arc);
  if (!g) return false;
  ctx.beginPath();
  ctx.arc(g.cx, g.cy, g.r, g.a1, g.a2, !g.ccw);
  ctx.stroke();
  return true;
}

export function paintArcPath(
  ctx: CanvasRenderingContext2D,
  arc: Arc3,
  opts: { fill?: string; stroke?: string; lineWidth?: number },
): boolean {
  const g = arcFrom3Points(arc);
  if (!g) return false;
  ctx.beginPath();
  ctx.arc(g.cx, g.cy, g.r, g.a1, g.a2, !g.ccw);
  if (opts.fill) {
    ctx.fillStyle = opts.fill;
    ctx.fill();
  }
  if (opts.stroke) {
    ctx.strokeStyle = opts.stroke;
    ctx.lineWidth = opts.lineWidth ?? 1;
    ctx.stroke();
  }
  return true;
}

export function drawArcPoints(
  ctx: CanvasRenderingContext2D,
  arc: Arc3,
  zoom: number,
  color = "#ffd166",
) {
  ctx.fillStyle = color;
  for (const p of [arc.p1, arc.p2, arc.p3]) {
    ctx.beginPath();
    ctx.arc(p.x, p.y, 5 / zoom, 0, Math.PI * 2);
    ctx.fill();
  }
}

export function translateArc(arc: Arc3, dx: number, dy: number): Arc3 {
  return {
    p1: { x: arc.p1.x + dx, y: arc.p1.y + dy },
    p2: { x: arc.p2.x + dx, y: arc.p2.y + dy },
    p3: { x: arc.p3.x + dx, y: arc.p3.y + dy },
  };
}

export function pointsNear(a: Point, b: Point, eps = 0.5): boolean {
  return Math.hypot(a.x - b.x, a.y - b.y) <= eps;
}

/** Unit direction of the line (p1 → p2). */
export function lineDirection(line: { p1: Point; p2: Point }): Point | null {
  const dx = line.p2.x - line.p1.x;
  const dy = line.p2.y - line.p1.y;
  const len = Math.hypot(dx, dy);
  if (len < 1e-8) return null;
  return { x: dx / len, y: dy / len };
}

function chordSide(j: Point, e: Point, p: Point): number {
  return (e.x - j.x) * (p.y - j.y) - (e.y - j.y) * (p.x - j.x);
}

function angleSweep(a0: number, a1: number, ccw: boolean): number {
  if (ccw) return normAngle(a1 - a0);
  return normAngle(a0 - a1);
}

function pointOnCircle(cx: number, cy: number, r: number, angle: number): Point {
  return { x: cx + r * Math.cos(angle), y: cy + r * Math.sin(angle) };
}

/**
 * Recompute arc p3 so the arc is tangent to a line at junction (p1 or p2).
 * `lineTangent` is the line direction at the shared point; `hintP3` keeps bulge side.
 */
export function arcWithTangentAtEndpoint(
  arc: Arc3,
  junction: "p1" | "p2",
  lineTangent: Point,
  hintP3: Point = arc.p3,
): Arc3 | null {
  const J = junction === "p1" ? arc.p1 : arc.p2;
  const E = junction === "p1" ? arc.p2 : arc.p1;
  const jex = J.x - E.x;
  const jey = J.y - E.y;
  const chordLenSq = jex * jex + jey * jey;
  if (chordLenSq < 1e-8) return null;

  const tLen = Math.hypot(lineTangent.x, lineTangent.y);
  if (tLen < 1e-8) return null;
  const T = { x: lineTangent.x / tLen, y: lineTangent.y / tLen };
  const hintSide = chordSide(J, E, hintP3);

  const tryTangent = (t: Point): Arc3 | null => {
    const normals: Point[] = [
      { x: -t.y, y: t.x },
      { x: t.y, y: -t.x },
    ];
    for (const N of normals) {
      const denom = 2 * (N.x * jex + N.y * jey);
      if (Math.abs(denom) < 1e-8) continue;
      const dist = -chordLenSq / denom;
      const cx = J.x + dist * N.x;
      const cy = J.y + dist * N.y;
      const r = Math.hypot(J.x - cx, J.y - cy);
      if (r < 1e-4) continue;

      const aJ = Math.atan2(J.y - cy, J.x - cx);
      const aE = Math.atan2(E.y - cy, E.x - cx);

      for (const ccw of [true, false]) {
        const sweep = angleSweep(aJ, aE, ccw);
        if (sweep < 1e-4) continue;
        const aMid = ccw ? aJ + sweep * 0.5 : aJ - sweep * 0.5;
        const p3 = pointOnCircle(cx, cy, r, aMid);
        const side = chordSide(J, E, p3);
        if (hintSide !== 0 && side !== 0 && Math.sign(side) !== Math.sign(hintSide)) {
          continue;
        }
        const ordered =
          junction === "p1" ? { p1: J, p2: E, p3 } : { p1: E, p2: J, p3 };
        if (arcFrom3Points(ordered)) return ordered;
      }
    }
    return null;
  };

  return tryTangent(T) ?? tryTangent({ x: -T.x, y: -T.y });
}

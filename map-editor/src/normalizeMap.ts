import type { Arc3, GoalZone, MapFile, MapShape, MapTeamSpawns, Point, ShapeStyle, ZoneShape } from "./types";
import { emptyTeamSpawns } from "./types";
import { readOptionalTag } from "./objectTags";
import { applyFixedMapFrame } from "./mapFrame";
import { normalizeBackground } from "./background";
import { migratePlayfieldToColliders, type IceBounds } from "./colliderPresets";

function isPoint(v: unknown): v is Point {
  return (
    typeof v === "object" &&
    v !== null &&
    typeof (v as Point).x === "number" &&
    typeof (v as Point).y === "number"
  );
}

function readPoint(v: unknown, fallback: Point): Point {
  return isPoint(v) ? { x: v.x, y: v.y } : fallback;
}

function readOptionalPoint(v: unknown): Point | null {
  if (v == null) return null;
  return isPoint(v) ? { x: v.x, y: v.y } : null;
}

function readArc(v: unknown): Arc3 | null {
  if (typeof v !== "object" || v === null) return null;
  const o = v as Record<string, unknown>;
  if (!isPoint(o.p1) || !isPoint(o.p2) || !isPoint(o.p3)) return null;
  return { p1: o.p1, p2: o.p2, p3: o.p3 };
}

export function normalizeGoalZone(v: unknown): GoalZone | null {
  if (v == null) return null;
  if (typeof v !== "object") return null;
  const o = v as Record<string, unknown>;
  const locked = o.locked === true ? true : undefined;
  if (o.kind === "arc") {
    const arc = readArc(o);
    return arc
      ? { kind: "arc", arc, tag: readOptionalTag(o), ...(locked ? { locked: true } : {}) }
      : null;
  }
  if (o.kind === "rect" && typeof o.rect === "object" && o.rect) {
    const r = o.rect as Record<string, number>;
    return {
      kind: "rect",
      rect: { x: r.x, y: r.y, w: r.w, h: r.h },
      tag: readOptionalTag(o),
      ...(locked ? { locked: true } : {}),
    };
  }
  if ("w" in o && "h" in o) {
    return {
      kind: "rect",
      rect: {
        x: Number(o.x) || 0,
        y: Number(o.y) || 0,
        w: Number(o.w) || 0,
        h: Number(o.h) || 0,
      },
      tag: readOptionalTag(o),
      ...(locked ? { locked: true } : {}),
    };
  }
  return null;
}

function readOptionalZ(o: Record<string, unknown>): number | undefined {
  if (typeof o.z === "number" && Number.isFinite(o.z)) return o.z;
  return undefined;
}

function readOptionalLocked(o: Record<string, unknown>): boolean | undefined {
  return o.locked === true ? true : undefined;
}

function readShapeStyle(o: Record<string, unknown>): ShapeStyle {
  const style: ShapeStyle = {};
  if (typeof o.fill === "string" && o.fill.length > 0) style.fill = o.fill;
  if (typeof o.stroke === "string" && o.stroke.length > 0) style.stroke = o.stroke;
  if (typeof o.strokeWidth === "number" && Number.isFinite(o.strokeWidth)) {
    style.strokeWidth = o.strokeWidth;
  }
  const z = readOptionalZ(o);
  if (z !== undefined) style.z = z;
  const tag = readOptionalTag(o);
  if (tag !== undefined) style.tag = tag;
  if (readOptionalLocked(o)) style.locked = true;
  return style;
}

function normalizeShape(v: unknown): MapShape | null {
  if (typeof v !== "object" || v === null) return null;
  const o = v as Record<string, unknown>;
  const style = readShapeStyle(o);
  if (o.kind === "rect" && typeof o.rect === "object" && o.rect) {
    const r = o.rect as Record<string, number>;
    const rect = { x: r.x, y: r.y, w: r.w, h: r.h };
    const cornerRadius =
      typeof o.cornerRadius === "number" && Number.isFinite(o.cornerRadius)
        ? o.cornerRadius
        : undefined;
    return {
      kind: "rect",
      rect,
      ...style,
      ...(cornerRadius !== undefined && cornerRadius > 0 ? { cornerRadius } : {}),
    };
  }
  if (o.kind === "circle") {
    return {
      kind: "circle",
      x: Number(o.x) || 0,
      y: Number(o.y) || 0,
      radius: Number(o.radius) || 0,
      ...style,
    };
  }
  if (o.kind === "line" && isPoint(o.p1) && isPoint(o.p2)) {
    return { kind: "line", p1: o.p1, p2: o.p2, ...style };
  }
  if (o.kind === "arc") {
    const arc = readArc(o.arc ?? o);
    return arc ? { kind: "arc", arc, ...style } : null;
  }
  return null;
}

function readTeamSpawns(v: unknown): MapTeamSpawns {
  const team = emptyTeamSpawns();
  if (typeof v !== "object" || v === null) return team;
  const o = v as Record<string, unknown>;
  team.goalie = readOptionalPoint(o.goalie);
  if (Array.isArray(o.skaters)) {
    for (let i = 0; i < 3 && i < o.skaters.length; i++) {
      team.skaters[i] = readOptionalPoint(o.skaters[i]);
    }
  }
  return team;
}

export function normalizeMapFile(raw: unknown): MapFile {
  const base = raw as MapFile;
  const sp = (raw as Record<string, unknown>).spawns as Record<string, unknown> | undefined;
  const slotsRaw = sp?.slots;
  const slots: (Point | null)[] = Array.from({ length: 4 }, (_, i) => {
    if (Array.isArray(slotsRaw) && slotsRaw[i]) {
      return readOptionalPoint(slotsRaw[i]);
    }
    return base.spawns?.slots?.[i] ?? null;
  });

  const rawObj = raw as Record<string, unknown>;

  let colliders = (base.colliders ?? []).map((c) => {
      if (c.kind === "arc" && "arc" in c) return c;
      if (c.kind === "line" && "p1" in c && "p2" in c) return c;
      if ((c as { kind?: string }).kind === "arc") {
        const o = c as Record<string, unknown>;
        const arc = readArc(c);
        if (arc) {
          return {
            kind: "arc" as const,
            arc,
            tag: readOptionalTag(o),
            ...(readOptionalLocked(o) ? { locked: true } : {}),
          };
        }
      }
      if ((c as { kind?: string }).kind === "line") {
        const o = c as Record<string, unknown>;
        if (isPoint(o.p1) && isPoint(o.p2)) {
          return {
            kind: "line" as const,
            p1: o.p1,
            p2: o.p2,
            tag: readOptionalTag(o),
            ...(readOptionalLocked(o) ? { locked: true } : {}),
          };
        }
      }
      if ((c as { kind?: string }).kind === "rect") {
        const o = c as Record<string, unknown>;
        if (typeof o.rect === "object" && o.rect) {
          const r = o.rect as Record<string, number>;
          const rect = { x: r.x, y: r.y, w: r.w, h: r.h };
          const cornerRadius =
            typeof o.cornerRadius === "number" && Number.isFinite(o.cornerRadius)
              ? o.cornerRadius
              : undefined;
          return {
            kind: "rect" as const,
            rect,
            tag: readOptionalTag(o),
            ...(cornerRadius !== undefined && cornerRadius > 0 ? { cornerRadius } : {}),
            ...(readOptionalLocked(o) ? { locked: true } : {}),
          };
        }
      }
      if ((c as { kind?: string }).kind === "circle") {
        const o = c as Record<string, unknown>;
        return {
          kind: "circle" as const,
          x: Number(o.x) || 0,
          y: Number(o.y) || 0,
          radius: Number(o.radius) || 0,
          tag: readOptionalTag(o),
          ...(readOptionalLocked(o) ? { locked: true } : {}),
        };
      }
      return c;
    });

  const legacyPlayfield = readLegacyPlayfield(rawObj.playfield);
  colliders = migratePlayfieldToColliders(legacyPlayfield, colliders);

  const shapesRaw = rawObj.shapes;
  const shapes: MapShape[] = Array.isArray(shapesRaw)
    ? shapesRaw.map(normalizeShape).filter((s): s is MapShape => s !== null)
    : (base.shapes ?? []);

  return applyFixedMapFrame({
    ...base,
    goal_west: normalizeGoalZone(rawObj.goal_west),
    goal_east: normalizeGoalZone(rawObj.goal_east),
    spawns: {
      puck_start: sp ? readOptionalPoint(sp.puck_start) : base.spawns.puck_start,
      puck_faceoff: sp ? readOptionalPoint(sp.puck_faceoff) : base.spawns.puck_faceoff,
      host: sp ? readOptionalPoint(sp.host) : base.spawns.host,
      slots,
      team_a: sp ? readTeamSpawns(sp.team_a) : (base.spawns?.team_a ?? emptyTeamSpawns()),
      team_b: sp ? readTeamSpawns(sp.team_b) : (base.spawns?.team_b ?? emptyTeamSpawns()),
    },
    shapes,
    colliders,
    background: normalizeBackground(rawObj.background, base),
  });
}

function readLegacyPlayfield(v: unknown): IceBounds | null {
  if (v == null || typeof v !== "object") return null;
  const o = v as Record<string, number>;
  if (
    typeof o.min_x !== "number" ||
    typeof o.max_x !== "number" ||
    typeof o.min_y !== "number" ||
    typeof o.max_y !== "number"
  ) {
    return null;
  }
  return { min_x: o.min_x, max_x: o.max_x, min_y: o.min_y, max_y: o.max_y };
}

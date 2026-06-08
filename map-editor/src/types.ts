export type DrawShape = "square" | "circle" | "line" | "arc";

export type Rect = { x: number; y: number; w: number; h: number };
export type Point = { x: number; y: number };

export type Arc3 = { p1: Point; p2: Point; p3: Point };

export type ZoneShape =
  | { kind: "rect"; rect: Rect; tag?: string; locked?: boolean }
  | { kind: "arc"; arc: Arc3; tag?: string; locked?: boolean };

export type GoalZone = ZoneShape;

export type VisualKind = "ice_tile" | "wall_frame" | "circle_mark" | "goal_sprite";

export type VisualPiece = {
  id: string;
  kind: VisualKind;
  dest: Rect;
  src?: Rect;
  use_src?: boolean;
  flip_x?: boolean;
  slice_l?: number;
  slice_t?: number;
  slice_r?: number;
  slice_b?: number;
  /** Draw order among visuals and color shapes (lower = behind). */
  z?: number;
  /** Human-readable label shown on the canvas. */
  tag?: string;
  /** Editor-only: geometry cannot be moved, resized, or deleted on canvas. */
  locked?: boolean;
};

export type Collider =
  | { kind: "rect"; rect: Rect; cornerRadius?: number; tag?: string; locked?: boolean }
  | { kind: "circle"; x: number; y: number; radius: number; tag?: string; locked?: boolean }
  | { kind: "arc"; arc: Arc3; tag?: string; locked?: boolean }
  | { kind: "line"; p1: Point; p2: Point; tag?: string; locked?: boolean };

export type ShapeStyle = {
  fill?: string;
  stroke?: string;
  strokeWidth?: number;
  /** Draw order among visuals and color shapes (lower = behind). */
  z?: number;
  /** Human-readable label shown on the canvas. */
  tag?: string;
  /** Editor-only: geometry cannot be moved, resized, or deleted on canvas. */
  locked?: boolean;
};

export type MapShape =
  | ({ kind: "rect"; rect: Rect; cornerRadius?: number } & ShapeStyle)
  | ({ kind: "circle"; x: number; y: number; radius: number } & ShapeStyle)
  | ({ kind: "line"; p1: Point; p2: Point } & ShapeStyle)
  | ({ kind: "arc"; arc: Arc3 } & ShapeStyle);

/** Full-bleed backdrop behind all map visuals (within dest, usually camera_bounds). */
export type MapBackground = {
  /** CSS color (e.g. #112233 or rgba(...)). */
  color?: string;
  /** Image path under game resources/ (e.g. maps/backgrounds/arena.png). */
  image?: string;
  /** World rect the background fills. Defaults to camera_bounds. */
  dest?: Rect;
  /** Optional crop within the image (pixels). */
  src?: Rect;
  /** Tile the image to cover dest. */
  repeat?: boolean;
};

export type MapTeamSpawns = {
  goalie: Point | null;
  skaters: [Point | null, Point | null, Point | null];
};

export function emptyTeamSpawns(): MapTeamSpawns {
  return { goalie: null, skaters: [null, null, null] };
}

export type MapFile = {
  version: number;
  name: string;
  world_per_tex: number;
  player_bounds: Rect;
  camera_bounds: Rect;
  goal_west: GoalZone | null;
  goal_east: GoalZone | null;
  spawns: {
    puck_start: Point | null;
    puck_faceoff: Point | null;
    host: Point | null;
    slots: (Point | null)[];
    team_a: MapTeamSpawns;
    team_b: MapTeamSpawns;
  };
  visuals: VisualPiece[];
  shapes: MapShape[];
  colliders: Collider[];
  background?: MapBackground | null;
};

export type Tool =
  | "select"
  | "color_shape"
  | "ice_tile"
  | "wall_frame"
  | "circle_mark"
  | "goal_sprite"
  | "collider"
  | "goal_west"
  | "goal_east"
  | "spawn_puck"
  | "spawn_team_a_gk"
  | "spawn_team_a_sk0"
  | "spawn_team_a_sk1"
  | "spawn_team_a_sk2"
  | "spawn_team_b_gk"
  | "spawn_team_b_sk0"
  | "spawn_team_b_sk1"
  | "spawn_team_b_sk2";

export type SpawnKey =
  | "puck_start"
  | "puck_faceoff"
  | "team_a_goalie"
  | "team_a_skater_0"
  | "team_a_skater_1"
  | "team_a_skater_2"
  | "team_b_goalie"
  | "team_b_skater_0"
  | "team_b_skater_1"
  | "team_b_skater_2"
  | "host"
  | `slot_${number}`;

export const ROSTER_SPAWN_KEYS: SpawnKey[] = [
  "team_a_goalie",
  "team_a_skater_0",
  "team_a_skater_1",
  "team_a_skater_2",
  "team_b_goalie",
  "team_b_skater_0",
  "team_b_skater_1",
  "team_b_skater_2",
];

export const SPAWN_TOOL_KEYS: Partial<Record<Tool, SpawnKey>> = {
  spawn_puck: "puck_start",
  spawn_team_a_gk: "team_a_goalie",
  spawn_team_a_sk0: "team_a_skater_0",
  spawn_team_a_sk1: "team_a_skater_1",
  spawn_team_a_sk2: "team_a_skater_2",
  spawn_team_b_gk: "team_b_goalie",
  spawn_team_b_sk0: "team_b_skater_0",
  spawn_team_b_sk1: "team_b_skater_1",
  spawn_team_b_sk2: "team_b_skater_2",
};

export function isSpawnTool(tool: Tool): boolean {
  return tool in SPAWN_TOOL_KEYS;
}

export function spawnToolToKey(tool: Tool): SpawnKey | null {
  return SPAWN_TOOL_KEYS[tool] ?? null;
}

export function spawnKeyLabel(key: SpawnKey): string {
  switch (key) {
    case "puck_start":
      return "Puck start";
    case "puck_faceoff":
      return "Puck faceoff";
    case "team_a_goalie":
      return "Team A goalie";
    case "team_a_skater_0":
      return "Team A skater 1";
    case "team_a_skater_1":
      return "Team A skater 2";
    case "team_a_skater_2":
      return "Team A skater 3";
    case "team_b_goalie":
      return "Team B goalie";
    case "team_b_skater_0":
      return "Team B skater 1";
    case "team_b_skater_1":
      return "Team B skater 2";
    case "team_b_skater_2":
      return "Team B skater 3";
    case "host":
      return "Host (legacy)";
    default:
      if (key.startsWith("slot_")) {
        return `Slot ${key.slice(5)} (legacy)`;
      }
      return key;
  }
}

export function getSpawnPoint(spawns: MapFile["spawns"], key: SpawnKey): Point | null {
  switch (key) {
    case "puck_start":
      return spawns.puck_start;
    case "puck_faceoff":
      return spawns.puck_faceoff;
    case "host":
      return spawns.host;
    case "team_a_goalie":
      return spawns.team_a.goalie;
    case "team_a_skater_0":
      return spawns.team_a.skaters[0];
    case "team_a_skater_1":
      return spawns.team_a.skaters[1];
    case "team_a_skater_2":
      return spawns.team_a.skaters[2];
    case "team_b_goalie":
      return spawns.team_b.goalie;
    case "team_b_skater_0":
      return spawns.team_b.skaters[0];
    case "team_b_skater_1":
      return spawns.team_b.skaters[1];
    case "team_b_skater_2":
      return spawns.team_b.skaters[2];
    default:
      if (key.startsWith("slot_")) {
        return spawns.slots[Number(key.slice(5))] ?? null;
      }
      return null;
  }
}

export function setSpawnPoint(
  spawns: MapFile["spawns"],
  key: SpawnKey,
  point: Point | null,
): void {
  switch (key) {
    case "puck_start":
      spawns.puck_start = point;
      break;
    case "puck_faceoff":
      spawns.puck_faceoff = point;
      break;
    case "host":
      spawns.host = point;
      break;
    case "team_a_goalie":
      spawns.team_a.goalie = point;
      break;
    case "team_a_skater_0":
      spawns.team_a.skaters[0] = point;
      break;
    case "team_a_skater_1":
      spawns.team_a.skaters[1] = point;
      break;
    case "team_a_skater_2":
      spawns.team_a.skaters[2] = point;
      break;
    case "team_b_goalie":
      spawns.team_b.goalie = point;
      break;
    case "team_b_skater_0":
      spawns.team_b.skaters[0] = point;
      break;
    case "team_b_skater_1":
      spawns.team_b.skaters[1] = point;
      break;
    case "team_b_skater_2":
      spawns.team_b.skaters[2] = point;
      break;
    default:
      if (key.startsWith("slot_")) {
        spawns.slots[Number(key.slice(5))] = point;
      }
  }
}

export function allSpawnChecks(spawns: MapFile["spawns"]): [SpawnKey, Point | null][] {
  return [
    ["puck_start", spawns.puck_start],
    ["puck_faceoff", spawns.puck_faceoff],
    ...ROSTER_SPAWN_KEYS.map((key) => [key, getSpawnPoint(spawns, key)] as [SpawnKey, Point | null]),
  ];
}

export type Selection =
  | { type: "visual"; index: number }
  | { type: "shape"; index: number }
  | { type: "collider"; index: number }
  | { type: "goal_west" }
  | { type: "goal_east" }
  | { type: "spawn"; key: SpawnKey };

export function selectionKey(sel: Selection): string {
  switch (sel.type) {
    case "visual":
      return `visual:${sel.index}`;
    case "shape":
      return `shape:${sel.index}`;
    case "collider":
      return `collider:${sel.index}`;
    case "spawn":
      return `spawn:${sel.key}`;
    default:
      return sel.type;
  }
}

export function selectionEquals(a: Selection, b: Selection): boolean {
  return selectionKey(a) === selectionKey(b);
}

export function rectsIntersect(a: Rect, b: Rect): boolean {
  return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y;
}

export function rectFromPoints(p0: Point, p1: Point): Rect {
  return {
    x: Math.min(p0.x, p1.x),
    y: Math.min(p0.y, p1.y),
    w: Math.abs(p1.x - p0.x),
    h: Math.abs(p1.y - p0.y),
  };
}

export const WALL_SLICES = { l: 56, t: 60, r: 57, b: 47 };
export const CIRCLE_SRC: Rect = { x: 162, y: 146, w: 928, h: 940 };

export const DRAW_SHAPES: DrawShape[] = ["square", "circle", "line", "arc"];

export const CLICK_SHAPES: DrawShape[] = ["line", "arc"];

export const DRAG_SHAPES: DrawShape[] = ["square", "circle"];

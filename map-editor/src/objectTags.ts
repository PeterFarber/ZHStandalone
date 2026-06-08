import {
  DEFAULT_EAST_GOAL_TAG,
  DEFAULT_WEST_GOAL_TAG,
} from "./teamColors";
import type {
  Collider,
  GoalZone,
  MapFile,
  MapShape,
  Selection,
  SpawnKey,
  VisualKind,
  VisualPiece,
} from "./types";
import { spawnKeyLabel } from "./types";

export function readOptionalTag(raw: Record<string, unknown>): string | undefined {
  if (typeof raw.tag === "string") {
    const trimmed = raw.tag.trim();
    if (trimmed.length > 0) return trimmed;
  }
  return undefined;
}

export function defaultVisualTag(kind: VisualKind): string {
  switch (kind) {
    case "ice_tile":
      return "Ice";
    case "wall_frame":
      return "Walls";
    case "circle_mark":
      return "Circle mark";
    case "goal_sprite":
      return "Goal art";
  }
}

export function defaultShapeTag(shape: MapShape["kind"]): string {
  switch (shape) {
    case "rect":
      return "Color rect";
    case "circle":
      return "Color circle";
    case "line":
      return "Color line";
    case "arc":
      return "Color arc";
  }
}

export function defaultColliderTag(collider: Collider["kind"]): string {
  switch (collider) {
    case "rect":
      return "Blocker";
    case "circle":
      return "Blocker circle";
    case "line":
      return "Blocker line";
    case "arc":
      return "Blocker arc";
  }
}

export function defaultGoalTag(side: "west" | "east"): string {
  return side === "west" ? DEFAULT_WEST_GOAL_TAG : DEFAULT_EAST_GOAL_TAG;
}

export function visualDisplayTag(piece: VisualPiece): string {
  return piece.tag?.trim() || piece.id?.trim() || defaultVisualTag(piece.kind);
}

export function shapeDisplayTag(shape: MapShape): string {
  return shape.tag?.trim() || defaultShapeTag(shape.kind);
}

export function colliderDisplayTag(collider: Collider): string {
  return collider.tag?.trim() || defaultColliderTag(collider.kind);
}

export function goalDisplayTag(zone: GoalZone | null, side: "west" | "east"): string {
  return zone?.tag?.trim() || defaultGoalTag(side);
}

export function selectionSupportsTag(sel: Selection): boolean {
  return sel.type !== "spawn";
}

export function objectTag(map: MapFile, sel: Selection): string {
  switch (sel.type) {
    case "visual": {
      const piece = map.visuals[sel.index];
      return piece ? visualDisplayTag(piece) : "Visual";
    }
    case "shape": {
      const shape = map.shapes[sel.index];
      return shape ? shapeDisplayTag(shape) : "Color shape";
    }
    case "collider": {
      const collider = map.colliders[sel.index];
      return collider ? colliderDisplayTag(collider) : "Blocker";
    }
    case "goal_west":
      return goalDisplayTag(map.goal_west, "west");
    case "goal_east":
      return goalDisplayTag(map.goal_east, "east");
    case "spawn":
      return spawnKeyLabel(sel.key);
  }
}

export function setObjectTag(map: MapFile, sel: Selection, tag: string | undefined): void {
  const trimmed = tag?.trim();
  const value = trimmed && trimmed.length > 0 ? trimmed : undefined;
  switch (sel.type) {
    case "visual": {
      const piece = map.visuals[sel.index];
      if (!piece) return;
      if (value) piece.tag = value;
      else delete piece.tag;
      return;
    }
    case "shape": {
      const shape = map.shapes[sel.index];
      if (!shape) return;
      if (value) shape.tag = value;
      else delete shape.tag;
      return;
    }
    case "collider": {
      const collider = map.colliders[sel.index];
      if (!collider) return;
      if (value) collider.tag = value;
      else delete collider.tag;
      return;
    }
    case "goal_west":
      if (!map.goal_west) return;
      if (value) map.goal_west.tag = value;
      else delete map.goal_west.tag;
      return;
    case "goal_east":
      if (!map.goal_east) return;
      if (value) map.goal_east.tag = value;
      else delete map.goal_east.tag;
      return;
    case "spawn":
      return;
  }
}

export function spawnTagColor(key: SpawnKey): string {
  if (key.startsWith("team_b")) return "#ff6b6b";
  if (key.startsWith("team_a")) return "#4dabff";
  return "#ffd166";
}

import type { MapFile, Selection } from "./types";

export function selectionSupportsLock(sel: Selection): boolean {
  return sel.type !== "spawn";
}

export function isObjectLocked(map: MapFile, sel: Selection): boolean {
  switch (sel.type) {
    case "visual":
      return map.visuals[sel.index]?.locked === true;
    case "shape":
      return map.shapes[sel.index]?.locked === true;
    case "collider":
      return map.colliders[sel.index]?.locked === true;
    case "goal_west":
      return map.goal_west?.locked === true;
    case "goal_east":
      return map.goal_east?.locked === true;
    case "spawn":
      return false;
  }
}

export function setObjectLocked(map: MapFile, sel: Selection, locked: boolean): void {
  if (!selectionSupportsLock(sel)) return;
  switch (sel.type) {
    case "visual": {
      const piece = map.visuals[sel.index];
      if (!piece) return;
      if (locked) piece.locked = true;
      else delete piece.locked;
      return;
    }
    case "shape": {
      const shape = map.shapes[sel.index];
      if (!shape) return;
      if (locked) shape.locked = true;
      else delete shape.locked;
      return;
    }
    case "collider": {
      const collider = map.colliders[sel.index];
      if (!collider) return;
      if (locked) collider.locked = true;
      else delete collider.locked;
      return;
    }
    case "goal_west":
      if (!map.goal_west) return;
      if (locked) map.goal_west.locked = true;
      else delete map.goal_west.locked;
      return;
    case "goal_east":
      if (!map.goal_east) return;
      if (locked) map.goal_east.locked = true;
      else delete map.goal_east.locked;
      return;
    case "spawn":
      return;
  }
}

import { objectTag } from "./objectTags";
import { isObjectLocked } from "./objectLocks";
import {
  allSpawnChecks,
  getSpawnPoint,
  type MapFile,
  type Selection,
} from "./types";

export type MapObjectListEntry = {
  selection: Selection;
  name: string;
  category: string;
  locked: boolean;
};

export function listMapObjects(map: MapFile): MapObjectListEntry[] {
  const out: MapObjectListEntry[] = [];

  for (let i = 0; i < map.visuals.length; i++) {
    const sel: Selection = { type: "visual", index: i };
    out.push({
      selection: sel,
      name: objectTag(map, sel),
      category: "Visuals",
      locked: isObjectLocked(map, sel),
    });
  }

  for (let i = 0; i < map.shapes.length; i++) {
    const sel: Selection = { type: "shape", index: i };
    out.push({
      selection: sel,
      name: objectTag(map, sel),
      category: "Color shapes",
      locked: isObjectLocked(map, sel),
    });
  }

  for (let i = 0; i < map.colliders.length; i++) {
    const sel: Selection = { type: "collider", index: i };
    out.push({
      selection: sel,
      name: objectTag(map, sel),
      category: "Blockers",
      locked: isObjectLocked(map, sel),
    });
  }

  if (map.goal_west) {
    const sel: Selection = { type: "goal_west" };
    out.push({
      selection: sel,
      name: objectTag(map, sel),
      category: "Goals",
      locked: isObjectLocked(map, sel),
    });
  }

  if (map.goal_east) {
    const sel: Selection = { type: "goal_east" };
    out.push({
      selection: sel,
      name: objectTag(map, sel),
      category: "Goals",
      locked: isObjectLocked(map, sel),
    });
  }

  for (const [key] of allSpawnChecks(map.spawns)) {
    if (!getSpawnPoint(map.spawns, key)) continue;
    const sel: Selection = { type: "spawn", key };
    out.push({
      selection: sel,
      name: objectTag(map, sel),
      category: "Spawns",
      locked: false,
    });
  }

  return out.sort(
    (a, b) =>
      a.category.localeCompare(b.category) ||
      a.name.localeCompare(b.name, undefined, { sensitivity: "base" }),
  );
}

import type { MapFile } from "./types";
import { emptyTeamSpawns } from "./types";
import { applyFixedMapFrame } from "./mapFrame";

/** Blank map used on startup and by File → New. */
export function createDefaultMap(): MapFile {
  return applyFixedMapFrame({
    version: 1,
    name: "untitled",
    world_per_tex: 1.75,
    player_bounds: { x: 0, y: 0, w: 1920, h: 1080 },
    camera_bounds: { x: 0, y: 0, w: 2200, h: 1237.5 },
    goal_west: null,
    goal_east: null,
    spawns: {
      puck_start: null,
      puck_faceoff: null,
      host: null,
      slots: [null, null, null, null],
      team_a: emptyTeamSpawns(),
      team_b: emptyTeamSpawns(),
    },
    visuals: [],
    shapes: [],
    colliders: [],
  });
}

import type { MapFile } from "./types";
import { cameraBoundsFromPlayer, resolvePlayerBounds } from "./boundsUtils";

/** Fixed world scale — not editable in the editor. */
export const FIXED_WORLD_PER_TEX = 1.75;

/** Stamp world scale and recompute camera bounds from the map's play area. */
export function applyFixedMapFrame(map: MapFile): MapFile {
  const player_bounds = resolvePlayerBounds(map.player_bounds);
  return {
    ...map,
    world_per_tex: FIXED_WORLD_PER_TEX,
    player_bounds,
    camera_bounds: cameraBoundsFromPlayer(player_bounds),
  };
}

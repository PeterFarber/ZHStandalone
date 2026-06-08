import type { Rect } from "./types";

/** Match game design resolution (1920×1080). */
export const ASPECT_16_9 = 16 / 9;

export const DEFAULT_PLAYER_BOUNDS: Rect = { x: 0, y: 0, w: 1920, h: 1080 };

export const MIN_PLAY_AREA_W = 320;
export const MIN_PLAY_AREA_H = 180;

export function resolvePlayerBounds(player: Rect | undefined | null): Rect {
  if (player && player.w > 0 && player.h > 0) {
    return { x: player.x, y: player.y, w: player.w, h: player.h };
  }
  return { ...DEFAULT_PLAYER_BOUNDS };
}

export function playAreaCenter(player: Rect): { x: number; y: number } {
  return { x: player.x + player.w * 0.5, y: player.y + player.h * 0.5 };
}

/** Resize play area around its current center (keeps faceoff / center ice fixed). */
export function playAreaFromCenter(cx: number, cy: number, w: number, h: number): Rect {
  const width = Math.max(MIN_PLAY_AREA_W, Math.round(w));
  const height = Math.max(MIN_PLAY_AREA_H, Math.round(h));
  return { x: cx - width * 0.5, y: cy - height * 0.5, w: width, h: height };
}

export function cameraBoundsFromPlayer(player: Rect, margin = 140): Rect {
  return {
    x: player.x - margin,
    y: player.y - margin * (9 / 16),
    w: player.w + margin * 2,
    h: player.h + margin * 2 * (9 / 16),
  };
}

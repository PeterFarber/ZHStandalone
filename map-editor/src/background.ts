import type { MapBackground, MapFile, Rect } from "./types";
import { cameraBoundsFromPlayer, resolvePlayerBounds } from "./boundsUtils";

export function defaultBackgroundDest(map: MapFile): Rect {
  if (map.camera_bounds?.w > 0 && map.camera_bounds?.h > 0) {
    return { ...map.camera_bounds };
  }
  return cameraBoundsFromPlayer(resolvePlayerBounds(map.player_bounds));
}

export function effectiveBackgroundDest(map: MapFile): Rect {
  return map.background?.dest ?? defaultBackgroundDest(map);
}

export function readOptionalRect(v: unknown): Rect | undefined {
  if (typeof v !== "object" || v === null) return undefined;
  const o = v as Record<string, unknown>;
  if (typeof o.w !== "number" || typeof o.h !== "number") return undefined;
  return {
    x: Number(o.x) || 0,
    y: Number(o.y) || 0,
    w: o.w,
    h: o.h,
  };
}

export function normalizeBackground(raw: unknown, map: MapFile): MapBackground | null {
  if (raw == null) return null;
  if (typeof raw !== "object") return null;
  const o = raw as Record<string, unknown>;
  const bg: MapBackground = {};
  if (typeof o.color === "string" && o.color.trim().length > 0) {
    bg.color = o.color.trim();
  }
  if (typeof o.image === "string" && o.image.trim().length > 0) {
    bg.image = o.image.trim();
  }
  const dest = readOptionalRect(o.dest);
  if (dest) bg.dest = dest;
  const src = readOptionalRect(o.src);
  if (src) bg.src = src;
  if (o.repeat === true) bg.repeat = true;
  if (!bg.color && !bg.image) return null;
  if (!bg.dest) bg.dest = defaultBackgroundDest(map);
  return bg;
}

export function backgroundImageUrl(imagePath: string): string {
  if (
    imagePath.startsWith("http://") ||
    imagePath.startsWith("https://") ||
    imagePath.startsWith("data:") ||
    imagePath.startsWith("blob:")
  ) {
    return imagePath;
  }
  if (imagePath.startsWith("/")) return imagePath;
  return `/resources/${imagePath.replace(/^\/+/, "")}`;
}

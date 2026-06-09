import type { Rect } from "./types";

/** Match game window / design resolution (16:9). */
export const DESIGN_VIEWPORT_W = 1280;
export const DESIGN_VIEWPORT_H = 720;

/** Same limits as `include/zh/game/playing_camera.hpp`. */
export const MAX_VIEW_FRAC = 0.75;
export const MIN_VIEW_FRAC = 0.18;

export function clampViewFraction(frac: number): number {
  return Math.min(MAX_VIEW_FRAC, Math.max(MIN_VIEW_FRAC, frac));
}

/** Camera2D zoom for a view fraction of the play bounds. */
export function zoomForViewFraction(viewFrac: number, play: Rect): number {
  if (play.w <= 0 || play.h <= 0) return 1;
  const f = clampViewFraction(viewFrac);
  return Math.max(
    DESIGN_VIEWPORT_W / (f * play.w),
    DESIGN_VIEWPORT_H / (f * play.h),
  );
}

/** World-space rectangle visible on screen at the given zoom level (view fraction). */
export function worldViewRectAtFraction(
  centerX: number,
  centerY: number,
  viewFrac: number,
  play: Rect,
): Rect {
  const zoom = zoomForViewFraction(viewFrac, play);
  const w = DESIGN_VIEWPORT_W / zoom;
  const h = DESIGN_VIEWPORT_H / zoom;
  return { x: centerX - w * 0.5, y: centerY - h * 0.5, w, h };
}

export const CAMERA_PREVIEW_LEVELS: { frac: number; label: string; dashed: boolean }[] = [
  { frac: MAX_VIEW_FRAC, label: "75% zoom out (max)", dashed: false },
  { frac: MIN_VIEW_FRAC, label: "18% zoom in (max)", dashed: true },
];

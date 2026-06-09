#pragma once

// Playing-screen Camera2D zoom helpers from visible play-area fraction.

#include "zh/game/types.hpp"

#include <algorithm>

namespace zh::game {

// Most zoomed out: show nearly full rink width on large maps.
inline constexpr float kPlayingMaxVisiblePlayFraction = 0.88f;
// Default zoom at match start — moderate zoom-out so more of the rink is visible.
inline constexpr float kPlayingDefaultViewFraction = 0.55f;
// Most zoomed in: smallest play-area fraction shown at once.
inline constexpr float kPlayingMinVisiblePlayFraction = 0.14f;
// Mouse-wheel multiplicative step on view fraction (>1 = coarser steps).
inline constexpr float kPlayingViewFractionWheelStep = 1.12f;

[[nodiscard]] inline float clamp_playing_view_fraction(float frac) noexcept {
    return std::clamp(frac, kPlayingMinVisiblePlayFraction, kPlayingMaxVisiblePlayFraction);
}

// Camera2D zoom for showing `view_fraction` of the play bounds.
[[nodiscard]] inline float playing_zoom_for_view_fraction(float view_fraction,
                                                           float viewport_w,
                                                           float viewport_h,
                                                           RectF const &play_bounds) noexcept {
    if (play_bounds.w <= 0.f || play_bounds.h <= 0.f || viewport_w <= 0.f || viewport_h <= 0.f) {
        return 1.f;
    }
    float const f = clamp_playing_view_fraction(view_fraction);
    float const zoom_w = viewport_w / (f * play_bounds.w);
    float const zoom_h = viewport_h / (f * play_bounds.h);
    return std::max(zoom_w, zoom_h);
}

// Current view fraction implied by zoom (worst axis — the limiting one).
[[nodiscard]] inline float playing_view_fraction_for_zoom(float zoom,
                                                           float viewport_w,
                                                           float viewport_h,
                                                           RectF const &play_bounds) noexcept {
    if (play_bounds.w <= 0.f || play_bounds.h <= 0.f || zoom <= 0.f) {
        return kPlayingMaxVisiblePlayFraction;
    }
    float const frac_w = (viewport_w / zoom) / play_bounds.w;
    float const frac_h = (viewport_h / zoom) / play_bounds.h;
    return clamp_playing_view_fraction(std::max(frac_w, frac_h));
}

}  // namespace zh::game

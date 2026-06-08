#pragma once

// Playing-screen Camera2D zoom helpers from visible play-area fraction.

#include "zh/game/types.hpp"

#include <algorithm>
#include <cmath>

namespace zh::game {

// Most zoomed out: at most this fraction of `player_bounds` width/height visible per axis.
inline constexpr float kPlayingMaxVisiblePlayFraction = 0.75f;
// Most zoomed in: smallest play-area fraction shown at once.
inline constexpr float kPlayingMinVisiblePlayFraction = 0.18f;
// Mouse-wheel multiplicative step on view fraction (>1 = coarser steps).
inline constexpr float kPlayingViewFractionWheelStep = 1.12f;

[[nodiscard]] inline float clamp_playing_view_fraction(float frac) noexcept {
    return std::clamp(frac, kPlayingMinVisiblePlayFraction, kPlayingMaxVisiblePlayFraction);
}

// Raylib `Camera2D::zoom` for showing `view_fraction` of the play bounds.
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

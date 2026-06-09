#pragma once

// Rink world bounds and layout accessors via map_runtime (ice, goals, camera clamp).

#include "zh/game/map_runtime.hpp"
#include "zh/game/types.hpp"

#include <algorithm>

namespace zh::game {

inline constexpr int kDesignScreenW = 1920;
inline constexpr int kDesignScreenH = 1080;
static_assert(kDesignScreenW * 9 == kDesignScreenH * 16, "design resolution must be 16:9");
// Default window size at launch; runtime layout uses `GetScreenWidth()` / `GetScreenHeight()`.
inline constexpr int kScreenW = kDesignScreenW;
inline constexpr int kScreenH = kDesignScreenH;

inline constexpr float kWorldPerTex = 1.75f;

// Board thickness from `walls.png` 9-slice margins (texture px → world).
inline constexpr float kBoardWorldLeft = 56.f * kWorldPerTex;
inline constexpr float kBoardWorldTop = 60.f * kWorldPerTex;
inline constexpr float kBoardWorldRight = 57.f * kWorldPerTex;
inline constexpr float kBoardWorldBottom = 47.f * kWorldPerTex;
inline constexpr float kBoardWorldTh = kBoardWorldLeft;
inline constexpr float kIceTileWorld = 128.f * kWorldPerTex;

[[nodiscard]] inline RectF rink_ice_bounds() noexcept { return map_runtime().ice_bounds(); }

[[nodiscard]] inline float rink_min_x() noexcept {
    RectF const b = rink_ice_bounds();
    return b.x;
}
[[nodiscard]] inline float rink_max_x() noexcept {
    RectF const b = rink_ice_bounds();
    return b.x + b.w;
}
[[nodiscard]] inline float rink_min_y() noexcept {
    RectF const b = rink_ice_bounds();
    return b.y;
}
[[nodiscard]] inline float rink_max_y() noexcept {
    RectF const b = rink_ice_bounds();
    return b.y + b.h;
}
[[nodiscard]] inline float rink_mid_y() noexcept { return map_runtime().mid_y(); }
[[nodiscard]] inline float rink_center_x() noexcept { return map_runtime().center_x(); }

inline void clamp_point_to_rink_capsule(float &px, float &py, float const radius) noexcept {
    RectF const b = rink_ice_bounds();
    px = std::clamp(px, b.x + radius, b.x + b.w - radius);
    py = std::clamp(py, b.y + radius, b.y + b.h - radius);
}

[[nodiscard]] inline RectF left_goal_zone() noexcept { return map_runtime().left_goal_zone(); }
[[nodiscard]] inline RectF right_goal_zone() noexcept { return map_runtime().right_goal_zone(); }

[[nodiscard]] inline float rink_art_world_w() noexcept {
    return map_runtime().definition().camera_bounds.w;
}

[[nodiscard]] inline float rink_art_world_h() noexcept {
    return map_runtime().definition().camera_bounds.h;
}

inline void clamp_rink_camera_target(float &target_x,
                                     float &target_y,
                                     float const viewport_w,
                                     float const viewport_h,
                                     float const zoom) noexcept {
    map_runtime().clamp_camera_target(target_x, target_y, viewport_w, viewport_h, zoom);
}

}  // namespace zh::game

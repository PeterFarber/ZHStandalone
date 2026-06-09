#pragma once

// Playing-screen Camera3D derived from the same focus + zoom fraction as Camera2D.

#include "zh/game/playing_camera.hpp"
#include "zh/game/types.hpp"
#include "zh/gfx/gfx_constants.hpp"
#include "zh/ui/world_space_3d.hpp"

#include <algorithm>
#include <cmath>

namespace zh::game {

// Pitched nadir rig: slight tilt toward the near side of the rink (+sim Y / world +Z).
inline constexpr bool kPlayingCameraForceTopDown = false;

inline constexpr float kPlayingCameraPitchFromTopDeg = 20.f;
inline constexpr float kPlayingCameraFovyDeg = 34.f;

[[nodiscard]] inline Camera3D make_playing_camera_3d(float focus_x,
                                                     float focus_y,
                                                     float view_fraction,
                                                     float viewport_w,
                                                     float viewport_h,
                                                     RectF const &play_bounds,
                                                     bool clamp_view_fraction = true) noexcept {
    float const f =
        clamp_view_fraction
            ? clamp_playing_view_fraction(view_fraction)
            : std::max(view_fraction, kPlayingMinVisiblePlayFraction);
    float const zoom_w =
        play_bounds.w > 0.f ? viewport_w / (f * play_bounds.w) : 1.f;
    float const zoom_h =
        play_bounds.h > 0.f ? viewport_h / (f * play_bounds.h) : 1.f;
    float const zoom = std::max(zoom_w, zoom_h);
    float const visible_h = viewport_h > 0.f && zoom > 0.f ? viewport_h / zoom : play_bounds.h * f;

    float const half_fovy_rad = (kPlayingCameraFovyDeg * 0.5f) * DEG2RAD;
    float const tan_half = std::tan(half_fovy_rad);
    float const rig_height =
        visible_h > 1e-3f && tan_half > 1e-4f ? (visible_h * 0.5f) / tan_half : 420.f;

    Camera3D cam{};
    cam.target = zh::ui::sim_to_world3(focus_x, focus_y, zh::ui::kPlayingIceY);
    cam.fovy = kPlayingCameraFovyDeg;
    cam.projection = CAMERA_PERSPECTIVE;

    if (kPlayingCameraForceTopDown) {
        // Nadir view: eye on +Y axis; up aims along -world Z (sim +Y on screen).
        cam.position = Vector3{focus_x, rig_height, focus_y};
        cam.up = Vector3{0.f, 0.f, -1.f};
        return cam;
    }

    float const pitch_rad = kPlayingCameraPitchFromTopDeg * DEG2RAD;
    float const cos_p = std::cos(pitch_rad);
    float const sin_p = std::sin(pitch_rad);
    cam.position =
        Vector3{focus_x, rig_height * cos_p, focus_y + rig_height * sin_p};
    cam.up = Vector3{0.f, 1.f, 0.f};
    return cam;
}

}  // namespace zh::game

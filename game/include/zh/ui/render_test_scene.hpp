#pragma once

// Isolated playfield build-out — real map geometry one step at a time, playing camera.

#include "zh/game/playing_camera.hpp"
#include "zh/ui/playing_scene_3d.hpp"

#include "zh/gfx/gfx_compat.hpp"

namespace zh::ui {

// When true, the game boots into Screen::RenderTest instead of Home.
// Override with --home / --no-render-test, or force with --render-test / ZH_RENDER_TEST=1.
inline constexpr bool kBootRenderTestScene = false;

[[nodiscard]] bool boot_render_test_scene(int argc, char **argv) noexcept;

struct RenderTestScene {
    Camera3D camera_{};
    int build_step_{0};
    bool cumulative_mode_{true};
    bool use_free_camera_{true};
    bool show_ice_bounds_{false};
    float view_fraction_{zh::game::kPlayingDefaultViewFraction};
    float focus_x_{0.f};
    float focus_y_{0.f};

    float fly_x_{0.f};
    float fly_y_{120.f};
    float fly_z_{0.f};
    float fly_yaw_deg_{45.f};
    float fly_pitch_deg_{25.f};
    float fly_speed_{480.f};
    Vector2 prev_mouse_{};
    bool mouse_prev_valid_{false};
    PlayingScene3D playing_scene_{};
    bool waypoint_active_{false};
    float waypoint_x_{0.f};
    float waypoint_y_{0.f};

    void reset_camera() noexcept;
    void update(float frame_dt) noexcept;
    void draw_world() const noexcept;
    void draw_hud() const noexcept;

private:
    void update_playing_camera() noexcept;
    void update_free_camera(float frame_dt) noexcept;
    void sync_free_camera_from_current() noexcept;
    void update_step_input() noexcept;
    void update_waypoint_input() noexcept;
    void update_focus_pan(float frame_dt) noexcept;
    void reset_waypoint_demo() noexcept;
    void draw_origin_debug() const noexcept;
    void draw_ice_bounds_debug() const noexcept;
    void draw_puck_demo() const noexcept;
    void draw_opponent_demo() const noexcept;
    void draw_gameplay_fx_demo() const noexcept;
    void apply_demo_waypoint(PlayingWorldDraw &world) const noexcept;
    [[nodiscard]] PlayingEntityDraw host_entity_demo() const noexcept;
};

}  // namespace zh::ui

// Sample keyboard/mouse for Playing. Host uses this in sim_match; WAN client packs it into ClientInput.
#include "detail/app_context.hpp"

#include "zh/ui/world_space_3d.hpp"

namespace zh {

using namespace ::zh::game;

namespace {

std::uint8_t g_pick_log_rmb_prev = 0;

}  // namespace

std::uint8_t detail::AppContext::sample_input_bits() const {
    // Bits 0–3: up, down, left, right (legacy; mouse waypoints are primary movement).
    std::uint8_t m = 0;
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) {
        m |= 1;
    }
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) {
        m |= 2;
    }
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) {
        m |= 4;
    }
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) {
        m |= 8;
    }
    return m;
}

std::uint8_t detail::AppContext::sample_ability_bits() const {
    std::uint8_t a = 0;
    if (IsKeyDown(KEY_Z)) {
        a |= kClientAbilityZ;
    }
    if (IsKeyDown(KEY_X)) {
        a |= kClientAbilityX;
    }
    if (IsKeyDown(KEY_S)) {
        a |= kClientAbilitySBrake;
    }
    if (IsKeyDown(KEY_C)) {
        a |= kClientAbilityCOneTimer;
    }
    return a;
}

std::uint8_t detail::AppContext::sample_local_match_mouse(float &mouse_x_out, float &mouse_y_out) {
    Vector2 const mp = GetMousePosition();

    // Same pick path as RenderTestScene::update_waypoint_input().
    Vector2 sim{};
    bool pick_ok = false;
    if (playing_cam_3d_ready_) {
        pick_ok = zh::ui::screen_to_sim_ground(mp, playing_cam_3d_, sim);
    } else {
        pick_ok = playing_screen_to_world(mp, sim);
    }
    if (pick_ok) {
        playing_last_aim_x_ = sim.x;
        playing_last_aim_y_ = sim.y;
    }

    bool const rmb_down = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    bool const rmb_edge = rmb_down && (g_pick_log_rmb_prev & kClientMouseRmbHeld) == 0U;
    if (rmb_edge && (playing_pick_debug_ || !pick_ok)) {
        refresh_playing_pick_debug(mp, true, "RMB");
    }
    g_pick_log_rmb_prev = rmb_down ? kClientMouseRmbHeld : 0U;

    mouse_x_out = playing_last_aim_x_;
    mouse_y_out = playing_last_aim_y_;

    std::uint8_t m = 0;
    if (rmb_down) {
        m |= kClientMouseRmbHeld;
    }
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        m |= kClientMouseLmbHeld;
    }
    return m;
}

detail::LocalPlayingInputSample detail::AppContext::build_local_playing_input_fresh() {
    LocalPlayingInputSample s{};
    s.move_axes = sample_input_bits();
    s.ability_axes = sample_ability_bits();
    s.mouse_buttons = sample_local_match_mouse(s.mouse_x, s.mouse_y);
    return s;
}

void detail::AppContext::refresh_local_playing_input(float const /*frame_dt*/) {
    playing_input_cached_ = false;
    playing_input_cache_ = build_local_playing_input_fresh();
    playing_input_cached_ = true;
}

void detail::AppContext::invalidate_local_playing_input_cache() noexcept {
    playing_input_cached_ = false;
}

detail::LocalPlayingInputSample detail::AppContext::sample_local_playing_input() {
    if (playing_input_cached_) {
        return playing_input_cache_;
    }

    refresh_local_playing_input(GetFrameTime());
    return playing_input_cache_;
}

}  // namespace zh

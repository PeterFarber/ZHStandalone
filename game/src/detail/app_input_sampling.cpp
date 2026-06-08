// Sample keyboard/mouse for Playing. Host uses this in sim_match; WAN client packs it into ClientInput.
#include "detail/app_context.hpp"

#include "zh/game/constants.hpp"
#include "zh/ui/hit_test.hpp"

#include <algorithm>

namespace zh {

using namespace ::zh::game;

using zh::ui::point_in;

std::uint8_t detail::AppContext::sample_input_bits() const {
    // Bits 0–3: up, down, left, right (legacy; mouse waypoints are primary movement).
    std::uint8_t m = 0;
    if (IsKeyDown(KEY_UP)) {
        m |= 1;
    }
    if (IsKeyDown(KEY_DOWN)) {
        m |= 2;
    }
    if (IsKeyDown(KEY_LEFT)) {
        m |= 4;
    }
    if (IsKeyDown(KEY_RIGHT)) {
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

Rectangle detail::AppContext::playing_leave_bounds() const {
    float const ch = static_cast<float>(GetScreenHeight());
    float const leave_h = 52.f;
    float const margin = 8.f;
    return Rectangle{margin + 40.f, ch - leave_h - margin, 280.f, leave_h};
}

std::uint8_t detail::AppContext::sample_local_match_mouse(Rectangle leave,
                                                          float &mouse_x_out,
                                                          float &mouse_y_out) {
    Vector2 const mp = GetMousePosition();
    if (point_in(leave, mp)) {
        mouse_x_out = mp.x;
        mouse_y_out = mp.y;
        return 0;
    }

    Vector2 const world = playing_screen_to_world(mp);
    mouse_x_out = world.x;
    mouse_y_out = world.y;
    std::uint8_t m = 0;
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        m |= kClientMouseRmbClick;
    }
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        m |= kClientMouseLmbHeld;
    }
    return m;
}

detail::LocalPlayingInputSample detail::AppContext::sample_local_playing_input() {
    if (screen_ == Screen::Playing) {
        float focus_x = rink_center_x();
        float focus_y = rink_mid_y();
        resolve_playing_camera_focus(focus_x, focus_y);
        update_playing_camera(focus_x, focus_y, GetFrameTime());
    }

    Rectangle const leave = playing_leave_bounds();
    LocalPlayingInputSample s{};
    s.move_axes = sample_input_bits();
    s.ability_axes = sample_ability_bits();
    s.mouse_buttons = sample_local_match_mouse(leave, s.mouse_x, s.mouse_y);
    return s;
}

}  // namespace zh

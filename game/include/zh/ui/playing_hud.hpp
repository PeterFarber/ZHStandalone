#pragma once

// Zealot-style playing overlay: top scoreboard, ability bar, corner controls.

#include "zh/gfx/gfx_compat.hpp"

#include <cstdint>

namespace zh::ui {

struct PlayingHudModel {
    bool show_match{false};
    unsigned goals_west{0};
    unsigned goals_east{0};
    std::uint8_t match_phase{0};
    std::uint8_t match_period{1};
    std::uint16_t period_ticks_remaining{0};
    std::uint8_t faceoff_countdown_ticks{0};

    bool local_active{false};
    bool local_goalie{false};
    unsigned boost_ready{0};
    unsigned boost_max{1};
    float boost_recharge_sec{0.f};
    float one_timer_cd_sec{0.f};
    bool one_timer_active{false};
    float shield_active_frac{0.f};

    bool z_down{false};
    bool x_down{false};
    bool c_down{false};
    bool s_down{false};
    bool r_down{false};
    bool lmb_shoot{false};

    bool show_shoot_bar{false};
    float shoot_charge{0.f};
};

struct PlayingHudLayout {
    Rectangle menu_button{};
    Rectangle settings_button{};
};

[[nodiscard]] PlayingHudLayout draw_playing_hud(PlayingHudModel const &model) noexcept;

void draw_player_name_tag(Vector2 screen, char const *name, Color team_color) noexcept;

}  // namespace zh::ui

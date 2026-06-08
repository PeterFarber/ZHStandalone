#pragma once

// Shared Raylib widgets: buttons, player sprites, rink backdrop, debug stick hitbox.

#include "zh/ui/rink_sprites.hpp"

#include <raylib.h>

namespace zh::ui {

void draw_centered_rect_button(Rectangle r,
                               Color fill,
                               Color border,
                               char const *label,
                               int font_px,
                               bool hovered) noexcept;

void draw_team_player_sprite(Texture2D const &tex_skater_team_a,
                             Texture2D const &tex_skater_team_b,
                             Texture2D const &tex_goalie_team_a,
                             Texture2D const &tex_goalie_team_b,
                             bool textures_ready,
                             float cx,
                             float cy,
                             bool roster_team_b,
                             bool is_goalie,
                             float rim_radius_px,
                             float sprite_dest_px,
                             Color rim_col,
                             Color fallback_fill_main) noexcept;

void draw_arctic_arena_rink(RinkSpriteSet const &sprites) noexcept;

void draw_stick_hitbox(float sk_px,
                       float sk_py,
                       float sk_vx,
                       float sk_vy,
                       float face_x,
                       float face_y,
                       Color fill,
                       Color stroke) noexcept;

}  // namespace zh::ui

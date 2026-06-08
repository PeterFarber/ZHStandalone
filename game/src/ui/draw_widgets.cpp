// Shared Raylib widgets: rink backdrop, buttons, player sprites, debug stick hitbox.

#include "zh/ui/draw_widgets.hpp"

#include "zh/game/constants.hpp"
#include "zh/game/puck_physics.hpp"
#include "zh/ui/rink_sprites.hpp"

namespace zh::ui {

void draw_arctic_arena_rink(RinkSpriteSet const &sprites) noexcept {
    draw_modular_rink(sprites);
}

void draw_centered_rect_button(Rectangle const r,
                               Color const fill,
                               Color const border,
                               char const *const label,
                               int const font_px,
                               bool const hovered) noexcept {
    Color const fill_draw = hovered ? ColorBrightness(fill, 0.22f) : fill;
    Color const border_draw = hovered ? ColorBrightness(border, 0.42f) : border;
    float const thick = hovered ? 4.5f : 3.f;
    DrawRectangleRec(r, fill_draw);
    DrawRectangleLinesEx(r, thick, border_draw);
    Color const label_color = hovered ? RAYWHITE : ColorBrightness(RAYWHITE, -0.08f);
    int const lw = MeasureText(label, font_px);
    float const lx =
        r.x + (static_cast<float>(r.width) - static_cast<float>(lw)) * 0.5f;
    float const ly =
        r.y +
        (static_cast<float>(r.height) - static_cast<float>(font_px)) * 0.5f;
    DrawText(label, static_cast<int>(lx), static_cast<int>(ly), font_px, label_color);
}

void draw_team_player_sprite(Texture2D const &tex_skater_team_a,
                             Texture2D const &tex_skater_team_b,
                             Texture2D const &tex_goalie_team_a,
                             Texture2D const &tex_goalie_team_b,
                             bool const textures_ready,
                             float const cx,
                             float const cy,
                             bool const roster_team_b,
                             bool const is_goalie,
                             float const rim_radius_px,
                             float const sprite_dest_px,
                             Color const rim_col,
                             Color const fallback_fill_main) noexcept {
    Texture2D pick{};
    if (textures_ready) {
        if (is_goalie) {
            pick = roster_team_b ? tex_goalie_team_b : tex_goalie_team_a;
        } else {
            pick = roster_team_b ? tex_skater_team_b : tex_skater_team_a;
        }
    }
    if ((!textures_ready) || pick.id == 0) {
        DrawCircleV({cx, cy}, rim_radius_px, fallback_fill_main);
    } else {
        float const hw = sprite_dest_px * 0.5f;
        Rectangle const src_rec{0.f, 0.f, static_cast<float>(pick.width),
                                static_cast<float>(pick.height)};
        Rectangle dest_rec{cx - hw, cy - hw, sprite_dest_px, sprite_dest_px};
        DrawTexturePro(pick, src_rec, dest_rec, Vector2{}, 0.f, WHITE);
    }
    DrawCircleLinesV({cx, cy}, rim_radius_px, rim_col);
}

void draw_stick_hitbox(float const sk_px,
                       float const sk_py,
                       float const sk_vx,
                       float const sk_vy,
                       float const face_x,
                       float const face_y,
                       Color const fill,
                       Color const stroke) noexcept {
    float stick_x = 0.f;
    float stick_y = 0.f;
    zh::game::skater_stick_hitbox_center(sk_px, sk_py, sk_vx, sk_vy, face_x, face_y, stick_x,
                                         stick_y);
    DrawCircle(static_cast<int>(stick_x), static_cast<int>(stick_y), zh::game::kStickHitboxRadius,
               fill);
    DrawCircleLines(static_cast<int>(stick_x), static_cast<int>(stick_y),
                    zh::game::kStickHitboxRadius, stroke);
}

}  // namespace zh::ui

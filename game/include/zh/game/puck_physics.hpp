#pragma once

// Puck carry, pickup/steal hitboxes, shots, loose-puck rink bounce (host + client replay).

#include "zh/game/rink_layout.hpp"
#include "zh/game/types.hpp"
#include "zh/game/constants.hpp"
#include "zh/game/skater_physics.hpp"
#include "zh/protocol.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace zh::game {

void sync_puck_carried(float sk_px,
                       float sk_py,
                       float sk_vx,
                       float sk_vy,
                       float face_x,
                       float face_y,
                       float &puck_px,
                       float &puck_py,
                       float &puck_vx,
                       float &puck_vy) noexcept;

[[nodiscard]] bool free_puck_overlaps_skater(float sk_px,
                                               float sk_py,
                                               float puck_px,
                                               float puck_py,
                                               float skater_body_radius = kSkaterDrawRadius) noexcept;

void skater_stick_hitbox_center(float sk_px,
                                float sk_py,
                                float sk_vx,
                                float sk_vy,
                                float face_x,
                                float face_y,
                                float &out_cx,
                                float &out_cy) noexcept;

[[nodiscard]] bool circles_overlap(float ax,
                                   float ay,
                                   float radius_a,
                                   float bx,
                                   float by,
                                   float radius_b) noexcept;

[[nodiscard]] bool stick_hitbox_overlaps_skater_body(float attacker_px,
                                                       float attacker_py,
                                                       float attacker_vx,
                                                       float attacker_vy,
                                                       float attacker_face_x,
                                                       float attacker_face_y,
                                                       float victim_px,
                                                       float victim_py) noexcept;

// Loose puck — skater **body** or **stick** circle may pick up.
[[nodiscard]] bool skater_can_pickup_loose_puck(float sk_px,
                                                  float sk_py,
                                                  float sk_vx,
                                                  float sk_vy,
                                                  float face_x,
                                                  float face_y,
                                                  float puck_px,
                                                  float puck_py,
                                                  float skater_body_radius = kSkaterDrawRadius) noexcept;

// **`LMB`** steal — attacker **stick** hitbox must overlap victim **body** hitbox.
[[nodiscard]] bool skater_can_steal_carried_puck(float thief_px,
                                                 float thief_py,
                                                 float thief_vx,
                                                 float thief_vy,
                                                 float thief_face_x,
                                                 float thief_face_y,
                                                 float carrier_px,
                                                 float carrier_py) noexcept;

void resolve_free_puck_against_rink(float &px, float &py, float &vx, float &vy) noexcept;

// Board/wall bounce only — used for puck collision SFX (goal frames use metal SFX).
[[nodiscard]] bool resolve_free_puck_against_boards(float &px,
                                                    float &py,
                                                    float &vx,
                                                    float &vy) noexcept;

// Goal frame / post bounce — metal puck SFX.
[[nodiscard]] bool resolve_free_puck_against_goal_frames(float &px,
                                                          float &py,
                                                          float &vx,
                                                          float &vy) noexcept;

// Loose puck vs an active goalie shield bubble (radial reflect).
[[nodiscard]] bool try_reflect_loose_puck_off_shield(float &puck_x,
                                                     float &puck_y,
                                                     float &puck_vx,
                                                     float &puck_vy,
                                                     float goalie_x,
                                                     float goalie_y) noexcept;

[[nodiscard]] bool puck_goal_overlap(float puck_cx,
                                     float puck_cy,
                                     float puck_radius,
                                     RectF const &zone) noexcept;

void face_off_spawn_puck(float &px,
                         float &py,
                         float &vx,
                         float &vy,
                         std::int8_t &carrier) noexcept;

// After puck release — aim-based slap or in-reach steal blend (**legacy** single-click path).
void apply_skater_puck_steal_or_shoot(float sk_x,
                                      float sk_y,
                                      float puck_x,
                                      float puck_y,
                                      float &vx,
                                      float &vy,
                                      float cursor_x,
                                      float cursor_y,
                                      float slap_strength,
                                      float steal_damp,
                                      float steal_nudge_pull) noexcept;

[[nodiscard]] float shoot_charge_fraction(std::uint32_t charge_ticks) noexcept;

[[nodiscard]] float shoot_aim_distance_factor(float sk_x, float sk_y, float cursor_x, float cursor_y) noexcept;

// Release charged slap: hold power × cursor-distance scaling, aim toward cursor.
void apply_charged_puck_shot(float sk_x,
                             float sk_y,
                             float &vx,
                             float &vy,
                             float cursor_x,
                             float cursor_y,
                             float charge_fraction,
                             float impulse_min,
                             float impulse_max) noexcept;

// One-timer redirect: aim toward cursor, preserve current puck speed magnitude.
void apply_one_timer_redirect(float sk_x,
                              float sk_y,
                              float &vx,
                              float &vy,
                              float cursor_x,
                              float cursor_y) noexcept;

// Strip loose puck from another skater (**LMB** press while overlapping).
void apply_puck_strip_nudge(float puck_x,
                            float puck_y,
                            float &vx,
                            float &vy,
                            float cursor_x,
                            float cursor_y,
                            float steal_damp,
                            float steal_nudge_pull) noexcept;

inline void format_puck_carrier_line(char *buf, std::size_t buf_sz,
                                     std::int8_t puck_carrier) noexcept {
    if (buf_sz == 0U) {
        return;
    }
    if (puck_carrier == kPuckCarrierFree) {
        std::snprintf(buf, buf_sz,
                      "Puck: loose — body or stick hitbox picks up; L tap stick→body to steal");
    } else if (puck_carrier == kPuckCarrierHost) {
        std::snprintf(buf, buf_sz, "Puck: HOST carry — hold L charge slap · tap L on enemy to steal");
    } else if (puck_carrier >= 0 &&
               static_cast<std::size_t>(puck_carrier) < kRemoteSlots) {
        std::snprintf(buf, buf_sz, "Puck: slot %u carry — stick hitbox on you to steal",
                      static_cast<unsigned>(puck_carrier));
    } else {
        std::snprintf(buf, buf_sz, "Puck possession: replicate error");
    }
}

}  // namespace zh::game

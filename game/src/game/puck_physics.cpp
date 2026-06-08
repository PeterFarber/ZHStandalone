// Puck carry pose, pickup/steal hitboxes, shots, and loose-puck rink bounce.

#include "zh/game/puck_physics.hpp"

#include "zh/game/constants.hpp"
#include "zh/game/map_geometry.hpp"
#include "zh/game/map_runtime.hpp"
#include "zh/game/rink_layout.hpp"
#include "zh/game/skater_physics.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>

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
                       float &puck_vy) noexcept {
    float nx = 0.f;
    float ny = 0.f;
    skater_blade_axis(sk_vx, sk_vy, face_x, face_y, nx, ny);

    puck_px = sk_px + nx * kBladeAhead;
    puck_py = sk_py + ny * kBladeAhead;
    clamp_point_to_rink_capsule(puck_px, puck_py, kPuckDrawRadius);
    puck_vx = sk_vx;
    puck_vy = sk_vy;
}

bool free_puck_overlaps_skater(float sk_px,
                               float sk_py,
                               float puck_px,
                               float puck_py,
                               float skater_body_radius) noexcept {
    float const dx = puck_px - sk_px;
    float const dy = puck_py - sk_py;
    float const rr = kPuckDrawRadius + skater_body_radius - 3.5f;
    float const thresh = rr * rr;
    return dx * dx + dy * dy <= thresh;
}

void skater_stick_hitbox_center(float const sk_px,
                                float const sk_py,
                                float const sk_vx,
                                float const sk_vy,
                                float const face_x,
                                float const face_y,
                                float &out_cx,
                                float &out_cy) noexcept {
    float axis_x = 0.f;
    float axis_y = 0.f;
    skater_blade_axis(sk_vx, sk_vy, face_x, face_y, axis_x, axis_y);
    out_cx = sk_px + axis_x * kStickHitboxAhead;
    out_cy = sk_py + axis_y * kStickHitboxAhead;
}

bool circles_overlap(float const ax,
                     float const ay,
                     float const radius_a,
                     float const bx,
                     float const by,
                     float const radius_b) noexcept {
    float const dx = ax - bx;
    float const dy = ay - by;
    float const rr = radius_a + radius_b;
    return dx * dx + dy * dy <= rr * rr;
}

// --- stick hitbox vs body overlap (steal / pickup) ---

bool stick_hitbox_overlaps_skater_body(float const attacker_px,
                                       float const attacker_py,
                                       float const attacker_vx,
                                       float const attacker_vy,
                                       float const attacker_face_x,
                                       float const attacker_face_y,
                                       float const victim_px,
                                       float const victim_py) noexcept {
    float stick_x = 0.f;
    float stick_y = 0.f;
    skater_stick_hitbox_center(attacker_px, attacker_py, attacker_vx, attacker_vy, attacker_face_x,
                               attacker_face_y, stick_x, stick_y);
    return circles_overlap(stick_x, stick_y, kStickHitboxRadius, victim_px, victim_py,
                           kSkaterDrawRadius);
}

bool skater_can_pickup_loose_puck(float const sk_px,
                                  float const sk_py,
                                  float const sk_vx,
                                  float const sk_vy,
                                  float const face_x,
                                  float const face_y,
                                  float const puck_px,
                                  float const puck_py,
                                  float const skater_body_radius) noexcept {
    if (free_puck_overlaps_skater(sk_px, sk_py, puck_px, puck_py, skater_body_radius)) {
        return true;
    }
    float stick_x = 0.f;
    float stick_y = 0.f;
    skater_stick_hitbox_center(sk_px, sk_py, sk_vx, sk_vy, face_x, face_y, stick_x, stick_y);
    float const rr = kPuckDrawRadius + kStickHitboxRadius - 3.5f;
    float const dx = puck_px - stick_x;
    float const dy = puck_py - stick_y;
    return dx * dx + dy * dy <= rr * rr;
}

bool skater_can_steal_carried_puck(float const thief_px,
                                   float const thief_py,
                                   float const thief_vx,
                                   float const thief_vy,
                                   float const thief_face_x,
                                   float const thief_face_y,
                                   float const carrier_px,
                                   float const carrier_py) noexcept {
    return stick_hitbox_overlaps_skater_body(thief_px, thief_py, thief_vx, thief_vy, thief_face_x,
                                             thief_face_y, carrier_px, carrier_py);
}

void resolve_free_puck_against_rink(float &px, float &py, float &vx, float &vy) noexcept {
    resolve_circle_against_map_colliders(px, py, vx, vy, kPuckDrawRadius, 0.98f);
}

// Returns true when a board/wall collider reflected the puck.
[[nodiscard]] bool resolve_free_puck_against_boards(float &px,
                                                    float &py,
                                                    float &vx,
                                                    float &vy) noexcept {
    float const pre_vx = vx;
    float const pre_vy = vy;
    resolve_circle_against_board_colliders(px, py, vx, vy, kPuckDrawRadius, 0.98f);
    return std::hypot(vx - pre_vx, vy - pre_vy) > 28.f;
}

[[nodiscard]] bool resolve_free_puck_against_goal_frames(float &px,
                                                         float &py,
                                                         float &vx,
                                                         float &vy) noexcept {
    float const pre_vx = vx;
    float const pre_vy = vy;
    resolve_circle_against_goal_frame_colliders(px, py, vx, vy, kPuckDrawRadius, 0.98f);
    return std::hypot(vx - pre_vx, vy - pre_vy) > 28.f;
}

[[nodiscard]] bool try_reflect_loose_puck_off_shield(float &puck_x,
                                                     float &puck_y,
                                                     float &puck_vx,
                                                     float &puck_vy,
                                                     float const goalie_x,
                                                     float const goalie_y) noexcept {
    float dx = puck_x - goalie_x;
    float dy = puck_y - goalie_y;
    float const dist_sq = dx * dx + dy * dy;
    float const sum_r = kGoalieShieldRadius + kPuckDrawRadius;
    float const sum_r_sq = sum_r * sum_r;
    if (dist_sq >= sum_r_sq || dist_sq < 1e-8f) {
        return false;
    }

    float const dist = std::sqrt(dist_sq);
    float const nx = dx / dist;
    float const ny = dy / dist;
    float const pen = sum_r - dist;
    puck_x += nx * pen;
    puck_y += ny * pen;

    float const vdotn = puck_vx * nx + puck_vy * ny;
    if (vdotn >= 0.f) {
        return true;
    }
    puck_vx -= 2.f * vdotn * nx;
    puck_vy -= 2.f * vdotn * ny;
    puck_vx *= kGoalieShieldBounce;
    puck_vy *= kGoalieShieldBounce;
    return true;
}

bool puck_goal_overlap(float puck_cx,
                       float puck_cy,
                       float puck_radius,
                       RectF const &zone) noexcept {
    float const closest_x =
        std::clamp(puck_cx, zone.x, zone.x + zone.w);
    float const closest_y =
        std::clamp(puck_cy, zone.y, zone.y + zone.h);
    float const dx = puck_cx - closest_x;
    float const dy = puck_cy - closest_y;
    return dx * dx + dy * dy <= puck_radius * puck_radius;
}

void face_off_spawn_puck(float &px,
                         float &py,
                         float &vx,
                         float &vy,
                         std::int8_t &carrier) noexcept {
    carrier = kPuckCarrierFree;
    MapPoint const faceoff = map_runtime().puck_faceoff_or_default();
    px = faceoff.x;
    py = faceoff.y;
    vx = 0.f;
    vy = 0.f;
}

float shoot_charge_fraction(std::uint32_t const charge_ticks) noexcept {
    if (kShootChargeMaxTicks == 0U) {
        return 0.f;
    }
    float const frac =
        static_cast<float>(charge_ticks) / static_cast<float>(kShootChargeMaxTicks);
    return std::clamp(frac, 0.f, 1.f);
}

float shoot_aim_distance_factor(float const sk_x,
                                float const sk_y,
                                float const cursor_x,
                                float const cursor_y) noexcept {
    float const dist = std::hypot(cursor_x - sk_x, cursor_y - sk_y);
    if (kShootAimDistForMax <= 1.f) {
        return 1.f;
    }
    float const scaled = dist / kShootAimDistForMax;
    return std::clamp(scaled, kShootAimDistMinFactor, 1.f);
}

// Charged slap release and legacy single-click steal/shoot helpers.
void apply_charged_puck_shot(float const sk_x,
                             float const sk_y,
                             float &vx,
                             float &vy,
                             float const cursor_x,
                             float const cursor_y,
                             float charge_fraction,
                             float const impulse_min,
                             float const impulse_max) noexcept {
    float const aim_dx = cursor_x - sk_x;
    float const aim_dy = cursor_y - sk_y;
    float const aim_len2 = aim_dx * aim_dx + aim_dy * aim_dy;
    if (aim_len2 < 49.f) {
        return;
    }
    float const inv = 1.f / std::sqrt(aim_len2);
    float const sx = aim_dx * inv;
    float const sy = aim_dy * inv;

    charge_fraction = std::clamp(charge_fraction, 0.f, 1.f);
    float const base = impulse_min + (impulse_max - impulse_min) * charge_fraction;
    float const impulse = base * shoot_aim_distance_factor(sk_x, sk_y, cursor_x, cursor_y);
    vx = sx * impulse;
    vy = sy * impulse;
}

void apply_one_timer_redirect(float const sk_x,
                              float const sk_y,
                              float &vx,
                              float &vy,
                              float const cursor_x,
                              float const cursor_y) noexcept {
    float speed = std::hypot(vx, vy);
    if (speed < kPuckStopSpeed) {
        speed = kPuckStopSpeed;
    }

    float const aim_dx = cursor_x - sk_x;
    float const aim_dy = cursor_y - sk_y;
    float const aim_len = std::hypot(aim_dx, aim_dy);
    if (aim_len < 7.f) {
        return;
    }

    vx = aim_dx / aim_len * speed;
    vy = aim_dy / aim_len * speed;
}

void apply_puck_strip_nudge(float const puck_x,
                            float const puck_y,
                            float &vx,
                            float &vy,
                            float const cursor_x,
                            float const cursor_y,
                            float const steal_damp,
                            float const steal_nudge_pull) noexcept {
    vx *= steal_damp;
    vy *= steal_damp;

    float const to_cur_x = cursor_x - puck_x;
    float const to_cur_y = cursor_y - puck_y;
    float const pull_len = std::hypot(to_cur_x, to_cur_y);
    if (pull_len >= 22.f) {
        float const px = to_cur_x / pull_len;
        float const py = to_cur_y / pull_len;
        vx += px * steal_nudge_pull;
        vy += py * steal_nudge_pull;
    }
}

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
                                      float steal_nudge_pull) noexcept {
    float const puck_dx = puck_x - sk_x;
    float const puck_dy = puck_y - sk_y;
    float const puck_d2 = puck_dx * puck_dx + puck_dy * puck_dy;

    constexpr float kStickReach = 118.f;
    float const stick_sq = kStickReach * kStickReach;

    float const aim_dx = cursor_x - sk_x;
    float const aim_dy = cursor_y - sk_y;
    float const aim_len2 = aim_dx * aim_dx + aim_dy * aim_dy;
    if (aim_len2 < 49.f) {
        return;
    }
    float const inv = 1.f / std::sqrt(aim_len2);
    float const sx = aim_dx * inv;
    float const sy = aim_dy * inv;

    if (puck_d2 <= stick_sq) {
        vx *= steal_damp;
        vy *= steal_damp;

        float const to_cur_x = cursor_x - puck_x;
        float const to_cur_y = cursor_y - puck_y;
        float const pull_len = std::hypot(to_cur_x, to_cur_y);
        if (pull_len >= 22.f) {
            float const px = to_cur_x / pull_len;
            float const py = to_cur_y / pull_len;
            vx += px * steal_nudge_pull;
            vy += py * steal_nudge_pull;
        }
    } else {
        vx += sx * slap_strength;
        vy += sy * slap_strength;
    }
}

}  // namespace zh::game

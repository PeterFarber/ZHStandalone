// Skater movement, abilities, and map/rink collision (shared by host sim and client replay).

#include "zh/game/skater_physics.hpp"

#include "zh/game/constants.hpp"
#include "zh/game/map_geometry.hpp"
#include "zh/game/map_runtime.hpp"
#include "zh/game/rink_layout.hpp"
#include "zh/game/types.hpp"

#include <algorithm>
#include <cmath>

namespace zh::game {

namespace {

void reflect_velocity_along_normal(float &vx, float &vy, float const nx, float const ny,
                                   float const bounce) noexcept {
    float const vdotn = vx * nx + vy * ny;
    if (vdotn >= 0.f) {
        return;
    }
    vx -= 2.f * vdotn * nx;
    vy -= 2.f * vdotn * ny;
    vx *= bounce;
    vy *= bounce;
}

void resolve_circle_rect_bounce(float &px, float &py, float &vx, float &vy, float const radius,
                                RectF const &rect, float const bounce) noexcept {
    float const closest_x = std::clamp(px, rect.x, rect.x + rect.w);
    float const closest_y = std::clamp(py, rect.y, rect.y + rect.h);
    float dx = px - closest_x;
    float dy = py - closest_y;
    float dist_sq = dx * dx + dy * dy;
    float const r_sq = radius * radius;

    if (dist_sq >= r_sq) {
        return;
    }

    float nx = 0.f;
    float ny = 0.f;
    float pen = 0.f;

    if (dist_sq > 1e-6f) {
        float const dist = std::sqrt(dist_sq);
        nx = dx / dist;
        ny = dy / dist;
        pen = radius - dist;
    } else {
        float const to_left = px - rect.x;
        float const to_right = rect.x + rect.w - px;
        float const to_top = py - rect.y;
        float const to_bottom = rect.y + rect.h - py;
        float min_pen = to_left;
        nx = -1.f;
        ny = 0.f;
        if (to_right < min_pen) {
            min_pen = to_right;
            nx = 1.f;
            ny = 0.f;
        }
        if (to_top < min_pen) {
            min_pen = to_top;
            nx = 0.f;
            ny = -1.f;
        }
        if (to_bottom < min_pen) {
            min_pen = to_bottom;
            nx = 0.f;
            ny = 1.f;
        }
        pen = min_pen;
    }

    px += nx * pen;
    py += ny * pen;
    reflect_velocity_along_normal(vx, vy, nx, ny, bounce);
}

void clamp_velocity_to_cap(float &vx, float &vy, float const speed_cap) noexcept {
    float const spd = std::hypot(vx, vy);
    if (spd > speed_cap && spd > 1e-4f) {
        float const s = speed_cap / spd;
        vx *= s;
        vy *= s;
    }
}

}  // namespace

// --- map colliders + goal mouth bounce ---

void resolve_circle_against_board_colliders(float &px,
                                            float &py,
                                            float &vx,
                                            float &vy,
                                            float const radius,
                                            float const bounce) noexcept {
    for (MapCollider const &col : map_runtime().colliders()) {
        if (col.kind == MapColliderKind::Circle) {
            RectF bounds{col.rect.x - col.radius, col.rect.y - col.radius, col.radius * 2.f,
                         col.radius * 2.f};
            resolve_circle_rect_bounce(px, py, vx, vy, radius, bounds, bounce);
        } else if (col.kind == MapColliderKind::Arc) {
            resolve_circle_arc_bounce(px, py, vx, vy, radius, col.arc, bounce);
        } else if (col.kind == MapColliderKind::Line) {
            resolve_circle_line_bounce(px, py, vx, vy, radius, col.line_p1, col.line_p2, bounce);
        } else if (col.corner_radius > 1e-4f) {
            resolve_circle_round_rect_bounce(px, py, vx, vy, radius, col.rect, col.corner_radius,
                                             bounce);
        } else {
            resolve_circle_rect_bounce(px, py, vx, vy, radius, col.rect, bounce);
        }
    }
}

void resolve_circle_against_goal_frame_colliders(float &px,
                                                 float &py,
                                                 float &vx,
                                                 float &vy,
                                                 float const radius,
                                                 float const bounce) noexcept {
    if (auto const &west = map_runtime().west_goal()) {
        resolve_circle_against_goal_zone_enclosure(px, py, vx, vy, radius, bounce, *west, true);
    }
    if (auto const &east = map_runtime().east_goal()) {
        resolve_circle_against_goal_zone_enclosure(px, py, vx, vy, radius, bounce, *east, false);
    }
}

void resolve_circle_against_map_colliders(float &px,
                                          float &py,
                                          float &vx,
                                          float &vy,
                                          float const radius,
                                          float const bounce) noexcept {
    resolve_circle_against_board_colliders(px, py, vx, vy, radius, bounce);
    resolve_circle_against_goal_frame_colliders(px, py, vx, vy, radius, bounce);
}

namespace {

void resolve_skater_rink_collisions(float &px, float &py, float &vx, float &vy,
                                    float body_radius) noexcept {
    resolve_circle_against_map_colliders(px, py, vx, vy, body_radius, kSkaterWallBounceFactor);
}

}  // namespace

void clamp_waypoint_to_rink(float &x, float &y, float inset) noexcept {
    clamp_point_to_rink_capsule(x, y, inset);
}

void clamp_skater_into_rink(float &px, float &py, float radius) noexcept {
    clamp_point_to_rink_capsule(px, py, radius);
}

std::uint8_t boost_ticks_mirror_u8(std::uint32_t ticks) noexcept {
    return ticks > 255U ? static_cast<std::uint8_t>(255U)
                        : static_cast<std::uint8_t>(ticks);
}

// --- abilities and per-tick velocity integration ---

void try_apply_boost_impulse(float &vx,
                             float &vy,
                             float const face_x,
                             float const face_y,
                             float const max_speed,
                             std::uint32_t *const boost_cd_rem,
                             unsigned const boost_charge_count,
                             std::uint32_t &overspeed_rem_ticks,
                             bool const z_rising_pulse,
                             float const impulse_strength) noexcept {
    if (!z_rising_pulse || boost_cd_rem == nullptr || boost_charge_count == 0U) {
        return;
    }

    std::size_t use_charge = boost_charge_count;
    for (std::size_t i = 0; i < boost_charge_count; ++i) {
        if (boost_cd_rem[i] == 0U) {
            use_charge = i;
            break;
        }
    }
    if (use_charge >= boost_charge_count) {
        return;
    }

    float dir_x = 0.f;
    float dir_y = 0.f;
    float const spd = std::hypot(vx, vy);
    if (spd >= kSkaterBoostMinDirSpeed) {
        dir_x = vx / spd;
        dir_y = vy / spd;
    } else {
        float const fl = std::hypot(face_x, face_y);
        if (fl > 1e-3f) {
            dir_x = face_x / fl;
            dir_y = face_y / fl;
        } else {
            dir_x = 1.f;
            dir_y = 0.f;
        }
    }

    vx += dir_x * impulse_strength;
    vy += dir_y * impulse_strength;

    float const cap = max_speed + kSkaterBoostOverspeed;
    clamp_velocity_to_cap(vx, vy, cap);
    overspeed_rem_ticks = kSkaterBoostOverspeedDurationTicks;
    boost_cd_rem[use_charge] = kSkaterBoostCooldownTicks;
}

void set_facing_toward(float const from_x,
                       float const from_y,
                       float const to_x,
                       float const to_y,
                       float &face_x,
                       float &face_y) noexcept {
    float const dx = to_x - from_x;
    float const dy = to_y - from_y;
    float const len = std::hypot(dx, dy);
    if (len < 1e-3f) {
        return;
    }
    face_x = dx / len;
    face_y = dy / len;
}

void skater_blade_axis(float const vx,
                       float const vy,
                       float const face_x,
                       float const face_y,
                       float &axis_x,
                       float &axis_y) noexcept {
    (void)vx;
    (void)vy;
    axis_x = face_x;
    axis_y = face_y;
    float len = std::hypot(axis_x, axis_y);
    if (len < 1e-3f) {
        axis_x = 1.f;
        axis_y = 0.f;
    } else {
        axis_x /= len;
        axis_y /= len;
    }
}

void tick_boost_cooldown(std::uint32_t &cd_rem) noexcept {
    if (cd_rem > 0U) {
        --cd_rem;
    }
}

void tick_boost_cooldowns(std::uint32_t *const boost_cd_rem,
                            unsigned const boost_charge_count) noexcept {
    if (boost_cd_rem == nullptr) {
        return;
    }
    for (unsigned i = 0; i < boost_charge_count; ++i) {
        tick_boost_cooldown(boost_cd_rem[i]);
    }
}

void tick_slide_cooldown(std::uint32_t &cd_rem) noexcept {
    if (cd_rem > 0U) {
        --cd_rem;
    }
}

void tick_brake_cooldown(std::uint32_t &cd_rem) noexcept {
    tick_slide_cooldown(cd_rem);
}

void try_activate_one_timer(std::uint32_t &window_rem_ticks,
                            std::uint32_t &cd_rem_ticks,
                            bool const c_rising_pulse) noexcept {
    if (!c_rising_pulse || cd_rem_ticks != 0U || window_rem_ticks != 0U) {
        return;
    }
    window_rem_ticks = kSkaterOneTimerDurationTicks;
    cd_rem_ticks = kSkaterOneTimerCooldownTicks;
}

void tick_one_timer_window(std::uint32_t &window_rem_ticks) noexcept {
    if (window_rem_ticks > 0U) {
        --window_rem_ticks;
    }
}

float one_timer_fraction(std::uint32_t const window_rem_ticks) noexcept {
    if constexpr (kSkaterOneTimerDurationTicks == 0U) {
        return 0.f;
    }
    return std::clamp(static_cast<float>(window_rem_ticks) /
                        static_cast<float>(kSkaterOneTimerDurationTicks),
                    0.f, 1.f);
}

void try_activate_goalie_shield(std::uint32_t &shield_rem_ticks,
                                std::uint32_t &cd_rem_ticks,
                                bool const x_rising_pulse) noexcept {
    if (!x_rising_pulse || cd_rem_ticks != 0U || shield_rem_ticks != 0U) {
        return;
    }
    shield_rem_ticks = kGoalieShieldDurationTicks;
    cd_rem_ticks = kGoalieShieldCooldownTicks;
}

void tick_goalie_shield_window(std::uint32_t &shield_rem_ticks) noexcept {
    if (shield_rem_ticks > 0U) {
        --shield_rem_ticks;
    }
}

float goalie_shield_fraction(std::uint32_t const shield_rem_ticks) noexcept {
    if constexpr (kGoalieShieldDurationTicks == 0U) {
        return 0.f;
    }
    return std::clamp(static_cast<float>(shield_rem_ticks) /
                        static_cast<float>(kGoalieShieldDurationTicks),
                    0.f, 1.f);
}

void try_apply_brake_tap(float &vx,
                         float &vy,
                         bool &has_waypoint,
                         std::uint32_t &brake_cd_rem,
                         bool const s_rising_pulse) noexcept {
    if (!s_rising_pulse || brake_cd_rem != 0U) {
        return;
    }
    has_waypoint = false;
    vx *= kSkaterBrakeDrag;
    vy *= kSkaterBrakeDrag;
    float const sp = std::hypot(vx, vy);
    if (sp < kSkaterStopSpeed * 1.6f) {
        vx = 0.f;
        vy = 0.f;
    }
    brake_cd_rem = kSkaterBrakeCooldownTicks;
}

void try_apply_slide_stop(float &vx,
                          float &vy,
                          bool &has_waypoint,
                          float &slide_carry_speed,
                          std::uint32_t &slide_decel_ticks,
                          std::uint32_t &slide_cd_rem,
                          std::uint32_t &boost_overspeed_ticks,
                          bool const x_rising_pulse) noexcept {
    if (!x_rising_pulse || slide_cd_rem != 0U) {
        return;
    }

    float const sp = std::hypot(vx, vy);
    (void)sp;
    slide_carry_speed = 1.f;
    slide_decel_ticks = kSkaterSlideDecelTicks;
    has_waypoint = false;
    boost_overspeed_ticks = 0U;
    slide_cd_rem = kSkaterSlideCooldownTicks;
}

void tick_slide_decel(float &vx, float &vy, std::uint32_t &ticks_rem) noexcept {
    if (ticks_rem == 0U) {
        return;
    }
    vx *= kSkaterSlideDecelPerTick;
    vy *= kSkaterSlideDecelPerTick;
    --ticks_rem;
}

void apply_slide_carry_on_new_waypoint(float &vx,
                                       float &vy,
                                       float const face_x,
                                       float const face_y,
                                       float const max_speed,
                                       float &slide_carry_speed) noexcept {
    if (slide_carry_speed <= 1e-3f) {
        return;
    }
    float const fl = std::hypot(face_x, face_y);
    if (fl < 1e-3f) {
        slide_carry_speed = 0.f;
        return;
    }
    float const fx = face_x / fl;
    float const fy = face_y / fl;
    float const launch = max_speed * kSkaterSlideNextLegSpeedCap;
    vx = fx * launch;
    vy = fy * launch;
    slide_carry_speed = 0.f;
}

// Per-tick waypoint steering and coast friction (integrate position separately).
void step_skater_velocity(float &vx,
                          float &vy,
                          float const px,
                          float const py,
                          float const way_x,
                          float const way_y,
                          bool &has_waypoint,
                          float &face_x,
                          float &face_y,
                          float const accel,
                          float const max_speed,
                          float const friction_coast,
                          float const arrival_dist_sq,
                          float const arrival_speed,
                          float const dt,
                          std::uint32_t &boost_overspeed_ticks) noexcept {
    float const speed_cap = (boost_overspeed_ticks > 0U)
                                ? (max_speed + kSkaterBoostOverspeed)
                                : max_speed;

    if (has_waypoint) {
        set_facing_toward(px, py, way_x, way_y, face_x, face_y);

        float const dx = way_x - px;
        float const dy = way_y - py;
        float const dist_sq = dx * dx + dy * dy;
        if (dist_sq > 1.f) {
            float const len = std::sqrt(dist_sq);
            float const ux = dx / len;
            float const uy = dy / len;
            vx += ux * accel * dt;
            vy += uy * accel * dt;
            clamp_velocity_to_cap(vx, vy, speed_cap);
        }

        float const spd = std::hypot(vx, vy);
        if (dist_sq <= arrival_dist_sq && spd < arrival_speed) {
            vx = 0.f;
            vy = 0.f;
            has_waypoint = false;
        }
    } else {
        vx *= friction_coast;
        vy *= friction_coast;
        clamp_velocity_to_cap(vx, vy, speed_cap);
        float const sp = std::hypot(vx, vy);
        if (sp < kSkaterStopSpeed) {
            vx = vy = 0.f;
        }
    }

    if (boost_overspeed_ticks > 0U) {
        --boost_overspeed_ticks;
    }
}

void integrate_skater_motion(float &px, float &py, float &vx, float &vy, float const dt) noexcept {
    integrate_skater_motion(px, py, vx, vy, dt, kSkaterDrawRadius);
}

void integrate_skater_motion(float &px,
                             float &py,
                             float &vx,
                             float &vy,
                             float const dt,
                             float const body_radius) noexcept {
    px += vx * dt;
    py += vy * dt;
    resolve_skater_rink_collisions(px, py, vx, vy, body_radius);
}

void step_skater_physics(float &px,
                         float &py,
                         float &vx,
                         float &vy,
                         float way_x,
                         float way_y,
                         bool &has_waypoint,
                         float &face_x,
                         float &face_y,
                         float accel,
                         float max_speed,
                         float friction_coast,
                         float arrival_dist_sq,
                         float arrival_speed,
                         float dt) noexcept {
    std::uint32_t unused_overspeed = 0U;
    step_skater_velocity(vx, vy, px, py, way_x, way_y, has_waypoint, face_x, face_y, accel,
                         max_speed, friction_coast, arrival_dist_sq, arrival_speed, dt,
                         unused_overspeed);
    integrate_skater_motion(px, py, vx, vy, dt);
}

}  // namespace zh::game

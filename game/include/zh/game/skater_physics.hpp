#pragma once

// Skater movement, abilities, map collider bounce (shared by host sim and client replay).

#include <cstdint>

namespace zh::game {

void resolve_circle_against_map_colliders(float &px,
                                          float &py,
                                          float &vx,
                                          float &vy,
                                          float const radius,
                                          float const bounce) noexcept;

void resolve_circle_against_board_colliders(float &px,
                                            float &py,
                                            float &vx,
                                            float &vy,
                                            float const radius,
                                            float const bounce) noexcept;

void resolve_circle_against_goal_frame_colliders(float &px,
                                                 float &py,
                                                 float &vx,
                                                 float &vy,
                                                 float const radius,
                                                 float const bounce) noexcept;

void clamp_waypoint_to_rink(float &x, float &y, float inset = 22.f) noexcept;
void clamp_skater_into_rink(float &px, float &py, float radius) noexcept;

[[nodiscard]] std::uint8_t boost_ticks_mirror_u8(std::uint32_t ticks) noexcept;

// Unit vector from skater toward a world point.
void set_facing_toward(float from_x,
                       float from_y,
                       float to_x,
                       float to_y,
                       float &face_x,
                       float &face_y) noexcept;

// Blade / stick axis follows stored **facing** (updated toward waypoint while **`has_waypoint`**).
void skater_blade_axis(float vx,
                       float vy,
                       float face_x,
                       float face_y,
                       float &axis_x,
                       float &axis_y) noexcept;

// **`Z`** rising edge while off cooldown — one-shot velocity impulse along motion (else facing).
void try_apply_boost_impulse(float &vx,
                             float &vy,
                             float face_x,
                             float face_y,
                             float max_speed,
                             std::uint32_t *boost_cd_rem,
                             unsigned boost_charge_count,
                             std::uint32_t &overspeed_rem_ticks,
                             bool z_rising_pulse,
                             float impulse_strength) noexcept;

void tick_boost_cooldowns(std::uint32_t *boost_cd_rem, unsigned boost_charge_count) noexcept;

void tick_slide_cooldown(std::uint32_t &cd_rem) noexcept;

void tick_brake_cooldown(std::uint32_t &cd_rem) noexcept;

void try_activate_one_timer(std::uint32_t &window_rem_ticks,
                            std::uint32_t &cd_rem_ticks,
                            bool c_rising_pulse) noexcept;

void try_activate_goalie_shield(std::uint32_t &shield_rem_ticks,
                                std::uint32_t &cd_rem_ticks,
                                bool x_rising_pulse) noexcept;

void tick_one_timer_window(std::uint32_t &window_rem_ticks) noexcept;

void tick_goalie_shield_window(std::uint32_t &shield_rem_ticks) noexcept;

[[nodiscard]] float one_timer_fraction(std::uint32_t window_rem_ticks) noexcept;

[[nodiscard]] float goalie_shield_fraction(std::uint32_t shield_rem_ticks) noexcept;

// **`S`** rising edge — one-shot brake; clears waypoint; **`kSkaterBrakeCooldownTicks`** between taps.
void try_apply_brake_tap(float &vx,
                         float &vy,
                         bool &has_waypoint,
                         std::uint32_t &brake_cd_rem,
                         bool s_rising_pulse) noexcept;

// **`X`** rising edge — bank speed for next leg, start gradual decel, cancel waypoint.
void try_apply_slide_stop(float &vx,
                          float &vy,
                          bool &has_waypoint,
                          float &slide_carry_speed,
                          std::uint32_t &slide_decel_ticks,
                          std::uint32_t &slide_cd_rem,
                          std::uint32_t &boost_overspeed_ticks,
                          bool x_rising_pulse) noexcept;

// After RMB sets a new leg — launch along **`face_*`** with banked slide speed if any.
void apply_slide_carry_on_new_waypoint(float &vx,
                                       float &vy,
                                       float face_x,
                                       float face_y,
                                       float max_speed,
                                       float &slide_carry_speed) noexcept;

// Gentle per-tick decel while a slide is active (**`ticks_rem`** counts down).
void tick_slide_decel(float &vx, float &vy, std::uint32_t &ticks_rem) noexcept;

// Waypoint: face click, raw accel toward it, cap speed — no trajectory / snap-to-dot.
void step_skater_velocity(float &vx,
                          float &vy,
                          float px,
                          float py,
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
                          float dt,
                          std::uint32_t &boost_overspeed_ticks) noexcept;

// Integrate position and resolve rink boards + goal frames (**reflects `vx`/`vy`** on hit).
void integrate_skater_motion(float &px, float &py, float &vx, float &vy, float dt) noexcept;

void integrate_skater_motion(float &px,
                             float &py,
                             float &vx,
                             float &vy,
                             float dt,
                             float body_radius) noexcept;

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
                         float dt) noexcept;

}  // namespace zh::game

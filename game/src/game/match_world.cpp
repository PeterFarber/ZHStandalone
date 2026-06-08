// Host authoritative match step: clock, skater/puck physics, goals and face-offs.

#include "zh/game/match_world.hpp"

#include "zh/game/constants.hpp"
#include "zh/game/map_geometry.hpp"
#include "zh/game/map_runtime.hpp"
#include "zh/game/puck_physics.hpp"
#include "zh/game/rink_layout.hpp"
#include "zh/game/skater_physics.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace zh::game {

// Lobby idle layout; screen size unused (spawns come from map_runtime).
void MatchWorld::reset_session(float const /*screen_w*/, float const /*screen_h*/) {
    server_tick = 0;
    puck_x = rink_center_x();
    puck_y = rink_mid_y();
    puck_vx = 0.f;
    puck_vy = 0.f;
    puck_carrier = kPuckCarrierFree;
    host_sk_vx = host_sk_vy = 0.f;

    MapPoint const host_spawn = map_runtime().host_spawn_or_default();
    host_sk_x = host_spawn.x;
    host_sk_y = host_spawn.y;
    host_has_waypoint = false;
    host_facing_x = 1.f;
    host_facing_y = 0.f;

    goals_left_side = 0;
    goals_right_side = 0;
    phase = MatchPhase::FaceoffCountdown;
    period = 1;
    period_ticks_remaining = kMatchPeriodDurationTicks;
    faceoff_countdown_ticks = 0;

    host_boost_active_ticks = 0;
    host_boost_cd_ticks = {};
    host_boost_overspeed_ticks = 0;
    host_slide_cd_ticks = 0;
    host_slide_decel_ticks = 0;
    host_slide_carry_speed = 0.f;
    host_brake_cd_ticks = 0;
    host_steal_cd_ticks = 0;
    host_pickup_block_ticks = 0;
    host_one_timer_ticks = 0;
    host_one_timer_cd_ticks = 0;
    host_shield_ticks = 0;
    host_shield_cd_ticks = 0;
    match_host_goalie = 0;
    match_slot_goalie_mask = 0;
    host_ability_prev = 0;
    host_shoot_cooldown = 0;
    host_mouse_prev = 0;
    host_shoot_charge_ticks = 0;

    for (std::size_t i = 0; i < kRemoteSlots; ++i) {
        MapPoint const slot_spawn = map_runtime().slot_spawn_or_default(i);
        slot_sk_x[i] = slot_spawn.x;
        slot_sk_y[i] = slot_spawn.y;
        slot_has_waypoint[i] = false;
        slot_ability_prev[i] = 0;
        slot_boost_active_ticks[i] = 0;
        slot_boost_cd_ticks[i] = {};
        slot_boost_overspeed_ticks[i] = 0;
        slot_slide_cd_ticks[i] = 0;
        slot_slide_decel_ticks[i] = 0;
        slot_slide_carry_speed[i] = 0.f;
        slot_brake_cd_ticks[i] = 0;
        slot_steal_cd_ticks[i] = 0;
        slot_pickup_block_ticks[i] = 0;
        slot_one_timer_ticks[i] = 0;
        slot_one_timer_cd_ticks[i] = 0;
        slot_shield_ticks[i] = 0;
        slot_shield_cd_ticks[i] = 0;
        slot_sk_vx[i] = 0.f;
        slot_sk_vy[i] = 0.f;
        slot_facing_x[i] = 1.f;
        slot_facing_y[i] = 0.f;
        slot_shoot_cooldown[i] = 0;
        slot_shoot_charge_ticks[i] = 0;
    }
}

// Snap host skaters to spawn points after face-off; inactive slots untouched.
void MatchWorld::apply_face_off_anchors(float const /*screen_w*/,
                                        float const /*screen_h*/,
                                        std::array<bool, kRemoteSlots> const &slot_active) noexcept {
    MapPoint const host_spawn = map_runtime().host_spawn_or_default();
    host_sk_x = host_spawn.x;
    host_sk_y = host_spawn.y;
    host_sk_vx = 0.f;
    host_sk_vy = 0.f;
    for (std::size_t i = 0; i < kRemoteSlots; ++i) {
        if (!slot_active[i]) {
            continue;
        }
        MapPoint const slot_spawn = map_runtime().slot_spawn_or_default(i);
        slot_sk_x[i] = slot_spawn.x;
        slot_sk_y[i] = slot_spawn.y;
        slot_sk_vx[i] = 0.f;
        slot_sk_vy[i] = 0.f;
    }
}

// Start button: reset scoreboard clock and drop puck for face-off countdown.
void MatchWorld::begin_match() noexcept {
    goals_left_side = 0;
    goals_right_side = 0;
    period = 1;
    period_ticks_remaining = kMatchPeriodDurationTicks;
    phase = MatchPhase::FaceoffCountdown;
    faceoff_countdown_ticks = kMatchFaceoffCountdownTicks;
    face_off_spawn_puck(puck_x, puck_y, puck_vx, puck_vy, puck_carrier);
    host_has_waypoint = false;
    host_sk_vx = 0.f;
    host_sk_vy = 0.f;
    for (std::size_t i = 0; i < kRemoteSlots; ++i) {
        slot_has_waypoint[i] = false;
        slot_sk_vx[i] = 0.f;
        slot_sk_vy[i] = 0.f;
    }
}

namespace {

// Shared path after a goal or inter-period break.
void start_faceoff_countdown(MatchWorld &world,
                             float const screen_w,
                             float const screen_h,
                             std::array<bool, kRemoteSlots> const &slot_active) noexcept {
    world.phase = MatchPhase::FaceoffCountdown;
    world.faceoff_countdown_ticks = kMatchFaceoffCountdownTicks;
    world.host_shield_ticks = 0U;
    world.host_shield_cd_ticks = 0U;
    for (std::size_t i = 0; i < kRemoteSlots; ++i) {
        world.slot_shield_ticks[i] = 0U;
        world.slot_shield_cd_ticks[i] = 0U;
    }
    face_off_spawn_puck(world.puck_x, world.puck_y, world.puck_vx, world.puck_vy, world.puck_carrier);
    world.host_has_waypoint = false;
    world.host_sk_vx = 0.f;
    world.host_sk_vy = 0.f;
    for (std::size_t i = 0; i < kRemoteSlots; ++i) {
        world.slot_has_waypoint[i] = false;
        world.slot_sk_vx[i] = 0.f;
        world.slot_sk_vy[i] = 0.f;
    }
    world.apply_face_off_anchors(screen_w, screen_h, slot_active);
}

}  // namespace

// Face-off countdown, period timer, and match end. Does not run gameplay physics.
void MatchWorld::tick_match_clock(std::array<bool, kRemoteSlots> const &slot_active,
                                  float const screen_w,
                                  float const screen_h) noexcept {
    if (phase == MatchPhase::MatchEnded) {
        return;
    }

    if (phase == MatchPhase::FaceoffCountdown) {
        if (faceoff_countdown_ticks > 0U) {
            --faceoff_countdown_ticks;
        }
        if (faceoff_countdown_ticks == 0U) {
            phase = MatchPhase::Playing;
        }
        return;
    }

    if (period_ticks_remaining > 0U) {
        --period_ticks_remaining;
    }
    if (period_ticks_remaining > 0U) {
        return;
    }

    if (period >= static_cast<std::uint8_t>(kMatchPeriodCount)) {
        phase = MatchPhase::MatchEnded;
        return;
    }

    ++period;
    period_ticks_remaining = kMatchPeriodDurationTicks;
    start_faceoff_countdown(*this, screen_w, screen_h, slot_active);
}

void MatchWorld::decrement_match_cooldowns() noexcept {
    if (host_shoot_cooldown > 0U) {
        --host_shoot_cooldown;
    }
    if (host_steal_cd_ticks > 0U) {
        --host_steal_cd_ticks;
    }
    if (host_pickup_block_ticks > 0U) {
        --host_pickup_block_ticks;
    }
    if (host_one_timer_cd_ticks > 0U) {
        --host_one_timer_cd_ticks;
    }
    if (host_shield_cd_ticks > 0U) {
        --host_shield_cd_ticks;
    }
    for (std::size_t i = 0; i < kRemoteSlots; ++i) {
        if (slot_shoot_cooldown[i] > 0U) {
            --slot_shoot_cooldown[i];
        }
        if (slot_steal_cd_ticks[i] > 0U) {
            --slot_steal_cd_ticks[i];
        }
        if (slot_pickup_block_ticks[i] > 0U) {
            --slot_pickup_block_ticks[i];
        }
        if (slot_one_timer_cd_ticks[i] > 0U) {
            --slot_one_timer_cd_ticks[i];
        }
        if (slot_shield_cd_ticks[i] > 0U) {
            --slot_shield_cd_ticks[i];
        }
    }
}

void MatchWorld::tick_end_boost_and_abilities(
    std::array<bool, kRemoteSlots> const &slot_active,
    std::uint8_t const host_ability_now,
    std::array<std::uint8_t, kRemoteSlots> const &slot_ability_now) {
    host_boost_active_ticks = 0;
    tick_boost_cooldowns(host_boost_cd_ticks.data(), kGoalieBoostCharges);
    tick_slide_cooldown(host_slide_cd_ticks);
    tick_brake_cooldown(host_brake_cd_ticks);
    tick_one_timer_window(host_one_timer_ticks);
    tick_goalie_shield_window(host_shield_ticks);
    for (std::size_t bi = 0; bi < kRemoteSlots; ++bi) {
        slot_boost_active_ticks[bi] = 0;
        if (slot_active[bi]) {
            tick_boost_cooldowns(slot_boost_cd_ticks[bi].data(), kGoalieBoostCharges);
            tick_slide_cooldown(slot_slide_cd_ticks[bi]);
            tick_brake_cooldown(slot_brake_cd_ticks[bi]);
            tick_one_timer_window(slot_one_timer_ticks[bi]);
            tick_goalie_shield_window(slot_shield_ticks[bi]);
        }
    }
    host_ability_prev = host_ability_now;
    for (std::size_t wi = 0; wi < kRemoteSlots; ++wi) {
        slot_ability_prev[wi] = slot_ability_now[wi];
    }
}

float MatchWorld::host_shoot_charge_fraction() const noexcept {
    return shoot_charge_fraction(host_shoot_charge_ticks);
}

float MatchWorld::host_one_timer_fraction() const noexcept {
    return one_timer_fraction(host_one_timer_ticks);
}

float MatchWorld::host_shield_fraction() const noexcept {
    return goalie_shield_fraction(host_shield_ticks);
}

namespace {

// Mouse edge helpers for steal and charged shot release.
bool mouse_lmb_held(std::uint8_t const bits) noexcept {
    return (bits & kClientMouseLmbHeld) != 0U;
}

bool mouse_lmb_rising(std::uint8_t const now_bits, std::uint8_t const prev_bits) noexcept {
    return mouse_lmb_held(now_bits) && !mouse_lmb_held(prev_bits);
}

void sample_carried_puck_pose(float sk_px,
                              float sk_py,
                              float sk_vx,
                              float sk_vy,
                              float face_x,
                              float face_y,
                              float &puck_px,
                              float &puck_py) noexcept {
    float puck_vx = 0.f;
    float puck_vy = 0.f;
    sync_puck_carried(sk_px, sk_py, sk_vx, sk_vy, face_x, face_y, puck_px, puck_py, puck_vx,
                      puck_vy);
}

bool try_steal_carried_puck(std::int8_t &puck_carrier,
                            float &puck_x,
                            float &puck_y,
                            float &puck_vx,
                            float &puck_vy,
                            float const thief_px,
                            float const thief_py,
                            float const thief_vx,
                            float const thief_vy,
                            float const thief_face_x,
                            float const thief_face_y,
                            float const cursor_x,
                            float const cursor_y,
                            std::int8_t const thief_carrier_id,
                            float const carrier_px,
                            float const carrier_py,
                            float const blade_puck_x,
                            float const blade_puck_y,
                            std::uint32_t &steal_cd_rem,
                            std::uint32_t &shoot_cd_rem,
                            std::uint32_t &shoot_charge_ticks) noexcept {
    if (steal_cd_rem != 0U) {
        return false;
    }
    if (puck_carrier == kPuckCarrierFree || puck_carrier == thief_carrier_id) {
        return false;
    }
    if (!skater_can_steal_carried_puck(thief_px, thief_py, thief_vx, thief_vy, thief_face_x,
                                       thief_face_y, carrier_px, carrier_py)) {
        return false;
    }

    puck_carrier = thief_carrier_id;
    puck_x = blade_puck_x;
    puck_y = blade_puck_y;
    puck_vx = thief_vx;
    puck_vy = thief_vy;
    apply_puck_strip_nudge(puck_x, puck_y, puck_vx, puck_vy, cursor_x, cursor_y, kMatchStealDamp,
                           kMatchStealNudge);
    steal_cd_rem = kPuckStealCooldownTicks;
    shoot_cd_rem = kShootAfterStealBlockTicks;
    shoot_charge_ticks = 0U;
    return true;
}

bool try_one_timer_loose_puck(float const sk_px,
                              float const sk_py,
                              float const sk_vx,
                              float const sk_vy,
                              float const face_x,
                              float const face_y,
                              float const cursor_x,
                              float const cursor_y,
                              std::uint32_t const window_rem_ticks,
                              float const skater_body_radius,
                              float &puck_x,
                              float &puck_y,
                              float &puck_vx,
                              float &puck_vy,
                              std::uint32_t &pickup_block_ticks) noexcept {
    if (window_rem_ticks == 0U) {
        return false;
    }
    if (!skater_can_pickup_loose_puck(sk_px, sk_py, sk_vx, sk_vy, face_x, face_y, puck_x, puck_y,
                                      skater_body_radius)) {
        return false;
    }
    apply_one_timer_redirect(sk_px, sk_py, puck_vx, puck_vy, cursor_x, cursor_y);
    pickup_block_ticks = kPuckSelfPickupBlockTicks;
    return true;
}

}  // namespace

// Main gameplay tick while phase == Playing.
HostMatchTickResult MatchWorld::tick_host_match(HostMatchTickInputs const &in,
                                                float const dt,
                                                std::array<bool, kRemoteSlots> const &slot_active,
                                                std::array<std::uint8_t, kRemoteSlots> &remote_mouse_seen,
                                                float const screen_w,
                                                float const screen_h) {
    HostMatchTickResult result{};

    if (phase != MatchPhase::Playing) {
        for (std::size_t i = 0; i < kRemoteSlots; ++i) {
            remote_mouse_seen[i] = in.slot_mouse_buttons[i];
        }
        host_mouse_prev = in.local.mouse_buttons;
        return result;
    }

    std::uint8_t const local_mouse_bits = in.local.mouse_buttons;
    float const local_mx = in.local.mouse_x;
    float const local_my = in.local.mouse_y;
    std::uint8_t const hab = in.local.ability_axes;

    if ((local_mouse_bits & kClientMouseRmbClick) != 0U) {
        float wx = local_mx;
        float wy = local_my;
        clamp_waypoint_to_rink(wx, wy);
        host_way_x = wx;
        host_way_y = wy;
        host_has_waypoint = true;
        set_facing_toward(host_sk_x, host_sk_y, wx, wy, host_facing_x, host_facing_y);
        apply_slide_carry_on_new_waypoint(host_sk_vx, host_sk_vy, host_facing_x, host_facing_y,
                                          player_max_speed((match_host_goalie & 1U) != 0U),
                                          host_slide_carry_speed);
        host_slide_decel_ticks = 0U;
    }

    for (std::size_t i = 0; i < kRemoteSlots; ++i) {
        if (!slot_active[i]) {
            continue;
        }
        bool const rmb_pulse_remote =
            ((in.slot_mouse_buttons[i] & kClientMouseRmbClick) != 0U) &&
            ((remote_mouse_seen[i] & kClientMouseRmbClick) == 0U);
        if (rmb_pulse_remote) {
            float wx = in.slot_mouse_x[i];
            float wy = in.slot_mouse_y[i];
            clamp_waypoint_to_rink(wx, wy);
            slot_way_x[i] = wx;
            slot_way_y[i] = wy;
            slot_has_waypoint[i] = true;
            set_facing_toward(slot_sk_x[i], slot_sk_y[i], wx, wy, slot_facing_x[i], slot_facing_y[i]);
            bool const slot_goalie_for_move =
                (match_slot_goalie_mask & static_cast<std::uint8_t>(1U << i)) != 0U;
            apply_slide_carry_on_new_waypoint(slot_sk_vx[i], slot_sk_vy[i], slot_facing_x[i],
                                              slot_facing_y[i], player_max_speed(slot_goalie_for_move),
                                              slot_slide_carry_speed[i]);
            slot_slide_decel_ticks[i] = 0U;
        }
    }

    bool const host_z_pulse =
        ((hab & kClientAbilityZ) != 0U) && ((host_ability_prev & kClientAbilityZ) == 0U);
    bool const host_x_pulse =
        ((hab & kClientAbilityX) != 0U) && ((host_ability_prev & kClientAbilityX) == 0U);
    bool const host_s_pulse =
        ((hab & kClientAbilitySBrake) != 0U) &&
        ((host_ability_prev & kClientAbilitySBrake) == 0U);
    bool const host_c_pulse =
        ((hab & kClientAbilityCOneTimer) != 0U) &&
        ((host_ability_prev & kClientAbilityCOneTimer) == 0U);
    try_activate_one_timer(host_one_timer_ticks, host_one_timer_cd_ticks, host_c_pulse);

    bool const host_is_goalie = (match_host_goalie & 1U) != 0U;
    step_skater_velocity(host_sk_vx, host_sk_vy, host_sk_x, host_sk_y, host_way_x, host_way_y,
                         host_has_waypoint, host_facing_x, host_facing_y, player_accel(host_is_goalie),
                         player_max_speed(host_is_goalie), kSkaterFrictionCoast, kWaypointArrivalDistSq,
                         kWaypointArrivalSpeed, dt, host_boost_overspeed_ticks);
    try_apply_brake_tap(host_sk_vx, host_sk_vy, host_has_waypoint, host_brake_cd_ticks,
                        host_s_pulse);
    if (host_is_goalie) {
        try_activate_goalie_shield(host_shield_ticks, host_shield_cd_ticks, host_x_pulse);
    } else {
        try_apply_slide_stop(host_sk_vx, host_sk_vy, host_has_waypoint, host_slide_carry_speed,
                             host_slide_decel_ticks, host_slide_cd_ticks, host_boost_overspeed_ticks,
                             host_x_pulse);
        tick_slide_decel(host_sk_vx, host_sk_vy, host_slide_decel_ticks);
    }
    try_apply_boost_impulse(host_sk_vx, host_sk_vy, host_facing_x, host_facing_y,
                            player_max_speed(host_is_goalie), host_boost_cd_ticks.data(),
                            player_boost_charge_count(host_is_goalie), host_boost_overspeed_ticks,
                            host_z_pulse, kSkaterBoostImpulse);
    if (host_boost_overspeed_ticks == kSkaterBoostOverspeedDurationTicks) {
        result.sounds.boost = true;
    }
    integrate_skater_motion(host_sk_x, host_sk_y, host_sk_vx, host_sk_vy, dt,
                          player_draw_radius(host_is_goalie));

    for (std::size_t i = 0; i < kRemoteSlots; ++i) {
        if (!slot_active[i]) {
            continue;
        }
        bool const slot_z_pulse =
            ((in.slot_ability[i] & kClientAbilityZ) != 0U) &&
            ((slot_ability_prev[i] & kClientAbilityZ) == 0U);
        bool const slot_x_pulse =
            ((in.slot_ability[i] & kClientAbilityX) != 0U) &&
            ((slot_ability_prev[i] & kClientAbilityX) == 0U);
        bool const slot_s_pulse =
            ((in.slot_ability[i] & kClientAbilitySBrake) != 0U) &&
            ((slot_ability_prev[i] & kClientAbilitySBrake) == 0U);
        bool const slot_c_pulse =
            ((in.slot_ability[i] & kClientAbilityCOneTimer) != 0U) &&
            ((slot_ability_prev[i] & kClientAbilityCOneTimer) == 0U);
        try_activate_one_timer(slot_one_timer_ticks[i], slot_one_timer_cd_ticks[i], slot_c_pulse);
        bool const slot_is_goalie =
            (match_slot_goalie_mask & static_cast<std::uint8_t>(1U << i)) != 0U;
        step_skater_velocity(slot_sk_vx[i], slot_sk_vy[i], slot_sk_x[i], slot_sk_y[i], slot_way_x[i],
                             slot_way_y[i], slot_has_waypoint[i], slot_facing_x[i],
                             slot_facing_y[i], player_accel(slot_is_goalie),
                             player_max_speed(slot_is_goalie), kSkaterFrictionCoast,
                             kWaypointArrivalDistSq, kWaypointArrivalSpeed, dt,
                             slot_boost_overspeed_ticks[i]);
        try_apply_brake_tap(slot_sk_vx[i], slot_sk_vy[i], slot_has_waypoint[i],
                            slot_brake_cd_ticks[i], slot_s_pulse);
        if (slot_is_goalie) {
            try_activate_goalie_shield(slot_shield_ticks[i], slot_shield_cd_ticks[i], slot_x_pulse);
        } else {
            try_apply_slide_stop(slot_sk_vx[i], slot_sk_vy[i], slot_has_waypoint[i],
                                 slot_slide_carry_speed[i], slot_slide_decel_ticks[i],
                                 slot_slide_cd_ticks[i], slot_boost_overspeed_ticks[i], slot_x_pulse);
            tick_slide_decel(slot_sk_vx[i], slot_sk_vy[i], slot_slide_decel_ticks[i]);
        }
        try_apply_boost_impulse(slot_sk_vx[i], slot_sk_vy[i], slot_facing_x[i], slot_facing_y[i],
                                player_max_speed(slot_is_goalie), slot_boost_cd_ticks[i].data(),
                                player_boost_charge_count(slot_is_goalie),
                                slot_boost_overspeed_ticks[i], slot_z_pulse, kSkaterBoostImpulse);
        if (slot_boost_overspeed_ticks[i] == kSkaterBoostOverspeedDurationTicks) {
            result.sounds.boost = true;
        }
        integrate_skater_motion(slot_sk_x[i], slot_sk_y[i], slot_sk_vx[i], slot_sk_vy[i], dt,
                                player_draw_radius(slot_is_goalie));
    }

    // Blade position for steal checks against whoever is carrying.
    float blade_puck_x = puck_x;
    float blade_puck_y = puck_y;
    float carrier_px = 0.f;
    float carrier_py = 0.f;
    if (puck_carrier == kPuckCarrierHost) {
        carrier_px = host_sk_x;
        carrier_py = host_sk_y;
        sample_carried_puck_pose(host_sk_x, host_sk_y, host_sk_vx, host_sk_vy, host_facing_x,
                                 host_facing_y, blade_puck_x, blade_puck_y);
    } else if (puck_carrier >= 0) {
        std::size_t const ci = static_cast<std::size_t>(puck_carrier);
        if (ci < kRemoteSlots && slot_active[ci]) {
            carrier_px = slot_sk_x[ci];
            carrier_py = slot_sk_y[ci];
            sample_carried_puck_pose(slot_sk_x[ci], slot_sk_y[ci], slot_sk_vx[ci], slot_sk_vy[ci],
                                     slot_facing_x[ci], slot_facing_y[ci], blade_puck_x,
                                     blade_puck_y);
        }
    }

    bool const host_lmb_held = mouse_lmb_held(local_mouse_bits);
    bool const host_lmb_was = mouse_lmb_held(host_mouse_prev);

    if (mouse_lmb_rising(local_mouse_bits, host_mouse_prev) && puck_carrier != kPuckCarrierFree &&
        puck_carrier != kPuckCarrierHost) {
        (void)try_steal_carried_puck(puck_carrier, puck_x, puck_y, puck_vx, puck_vy, host_sk_x,
                                     host_sk_y, host_sk_vx, host_sk_vy, host_facing_x, host_facing_y,
                                     local_mx, local_my, kPuckCarrierHost, carrier_px, carrier_py,
                                     blade_puck_x, blade_puck_y, host_steal_cd_ticks,
                                     host_shoot_cooldown, host_shoot_charge_ticks);
    }

    if (puck_carrier == kPuckCarrierHost) {
        if (host_lmb_held && host_shoot_cooldown == 0U) {
            if (host_shoot_charge_ticks < kShootChargeMaxTicks) {
                ++host_shoot_charge_ticks;
            }
        } else if (host_lmb_was && !host_lmb_held && host_shoot_charge_ticks > 0U) {
            float const charge_frac = shoot_charge_fraction(host_shoot_charge_ticks);
            puck_carrier = kPuckCarrierFree;
            apply_charged_puck_shot(host_sk_x, host_sk_y, puck_vx, puck_vy, local_mx, local_my,
                                    charge_frac, kShootImpulseMin, kShootImpulseMax);
            result.sounds.shot = true;
            host_shoot_cooldown = kShootCooldownTicks;
            host_shoot_charge_ticks = 0;
            host_pickup_block_ticks = kPuckSelfPickupBlockTicks;
        }
    } else if (!host_lmb_held) {
        host_shoot_charge_ticks = 0;
    }

    for (std::size_t i = 0; i < kRemoteSlots; ++i) {
        if (!slot_active[i]) {
            continue;
        }
        bool const slot_lmb_held = mouse_lmb_held(in.slot_mouse_buttons[i]);
        bool const slot_lmb_was = mouse_lmb_held(remote_mouse_seen[i]);

        if (mouse_lmb_rising(in.slot_mouse_buttons[i], remote_mouse_seen[i]) &&
            puck_carrier != kPuckCarrierFree &&
            puck_carrier != static_cast<std::int8_t>(i)) {
            (void)try_steal_carried_puck(puck_carrier, puck_x, puck_y, puck_vx, puck_vy,
                                         slot_sk_x[i], slot_sk_y[i], slot_sk_vx[i], slot_sk_vy[i],
                                         slot_facing_x[i], slot_facing_y[i], in.slot_mouse_x[i],
                                         in.slot_mouse_y[i], static_cast<std::int8_t>(i), carrier_px,
                                         carrier_py, blade_puck_x, blade_puck_y,
                                         slot_steal_cd_ticks[i], slot_shoot_cooldown[i],
                                         slot_shoot_charge_ticks[i]);
        }

        if (puck_carrier == static_cast<std::int8_t>(i)) {
            if (slot_lmb_held && slot_shoot_cooldown[i] == 0U) {
                if (slot_shoot_charge_ticks[i] < kShootChargeMaxTicks) {
                    ++slot_shoot_charge_ticks[i];
                }
            } else if (slot_lmb_was && !slot_lmb_held && slot_shoot_charge_ticks[i] > 0U) {
                float const charge_frac = shoot_charge_fraction(slot_shoot_charge_ticks[i]);
                puck_carrier = kPuckCarrierFree;
                apply_charged_puck_shot(slot_sk_x[i], slot_sk_y[i], puck_vx, puck_vy,
                                        in.slot_mouse_x[i], in.slot_mouse_y[i], charge_frac,
                                        kShootImpulseMin, kShootImpulseMax);
                result.sounds.shot = true;
                slot_shoot_cooldown[i] = kShootCooldownTicks;
                slot_shoot_charge_ticks[i] = 0;
                slot_pickup_block_ticks[i] = kPuckSelfPickupBlockTicks;
            }
        } else if (!slot_lmb_held) {
            slot_shoot_charge_ticks[i] = 0;
        }
    }

    // Loose puck: friction, walls, pickup race (host slot checked first).
    if (puck_carrier == kPuckCarrierFree) {
        float const max_spd_p = 720.f;
        puck_vx = std::clamp(puck_vx, -max_spd_p, max_spd_p);
        puck_vy = std::clamp(puck_vy, -max_spd_p, max_spd_p);
        puck_vx *= kPuckFloorFriction;
        puck_vy *= kPuckFloorFriction;
        float const ps = std::hypot(puck_vx, puck_vy);
        if (ps < kPuckStopSpeed) {
            puck_vx = 0.f;
            puck_vy = 0.f;
        }
        puck_x += puck_vx * dt;
        puck_y += puck_vy * dt;
        if (resolve_free_puck_against_boards(puck_x, puck_y, puck_vx, puck_vy)) {
            result.sounds.puck_collision = true;
        }
        if (resolve_free_puck_against_goal_frames(puck_x, puck_y, puck_vx, puck_vy)) {
            result.sounds.puck_metal_collision = true;
        }
        if ((match_host_goalie & 1U) != 0U && host_shield_ticks > 0U) {
            (void)try_reflect_loose_puck_off_shield(puck_x, puck_y, puck_vx, puck_vy, host_sk_x, host_sk_y);
        }
        for (std::size_t gi = 0; gi < kRemoteSlots; ++gi) {
            if (!slot_active[gi]) {
                continue;
            }
            if ((match_slot_goalie_mask & static_cast<std::uint8_t>(1U << gi)) == 0U) {
                continue;
            }
            if (slot_shield_ticks[gi] == 0U) {
                continue;
            }
            (void)try_reflect_loose_puck_off_shield(puck_x, puck_y, puck_vx, puck_vy, slot_sk_x[gi],
                                                    slot_sk_y[gi]);
        }

        bool const host_one_timed = try_one_timer_loose_puck(
            host_sk_x, host_sk_y, host_sk_vx, host_sk_vy, host_facing_x, host_facing_y, local_mx,
            local_my, host_one_timer_ticks, player_draw_radius(host_is_goalie), puck_x, puck_y,
            puck_vx, puck_vy, host_pickup_block_ticks);

        if (!host_one_timed && host_pickup_block_ticks == 0U &&
            skater_can_pickup_loose_puck(host_sk_x, host_sk_y, host_sk_vx, host_sk_vy, host_facing_x,
                                         host_facing_y, puck_x, puck_y,
                                         player_draw_radius(host_is_goalie))) {
            puck_carrier = kPuckCarrierHost;
        } else if (!host_one_timed) {
            for (std::size_t ii = 0; ii < kRemoteSlots; ++ii) {
                if (!slot_active[ii]) {
                    continue;
                }
                bool const slot_goalie_pickup =
                    (match_slot_goalie_mask & static_cast<std::uint8_t>(1U << ii)) != 0U;
                bool const slot_one_timed = try_one_timer_loose_puck(
                    slot_sk_x[ii], slot_sk_y[ii], slot_sk_vx[ii], slot_sk_vy[ii], slot_facing_x[ii],
                    slot_facing_y[ii], in.slot_mouse_x[ii], in.slot_mouse_y[ii],
                    slot_one_timer_ticks[ii], player_draw_radius(slot_goalie_pickup), puck_x, puck_y,
                    puck_vx, puck_vy, slot_pickup_block_ticks[ii]);
                if (slot_one_timed) {
                    break;
                }
                if (slot_pickup_block_ticks[ii] == 0U &&
                    skater_can_pickup_loose_puck(slot_sk_x[ii], slot_sk_y[ii], slot_sk_vx[ii],
                                                 slot_sk_vy[ii], slot_facing_x[ii], slot_facing_y[ii],
                                                 puck_x, puck_y, player_draw_radius(slot_goalie_pickup))) {
                    puck_carrier = static_cast<std::int8_t>(ii);
                    break;
                }
            }
        }
    }

    if (puck_carrier == kPuckCarrierHost) {
        sync_puck_carried(host_sk_x, host_sk_y, host_sk_vx, host_sk_vy, host_facing_x, host_facing_y,
                          puck_x, puck_y, puck_vx, puck_vy);
    } else if (puck_carrier >= 0) {
        std::size_t const si = static_cast<std::size_t>(puck_carrier);
        if (si < kRemoteSlots && slot_active[si]) {
            sync_puck_carried(slot_sk_x[si], slot_sk_y[si], slot_sk_vx[si], slot_sk_vy[si],
                              slot_facing_x[si], slot_facing_y[si], puck_x, puck_y, puck_vx, puck_vy);
        } else {
            puck_carrier = kPuckCarrierFree;
        }
    }

    // Goal detection; both nets overlapping picks side by puck x.
    bool const in_l = map_runtime().west_goal() &&
                      puck_overlaps_goal_zone(puck_x, puck_y, kPuckDrawRadius, *map_runtime().west_goal());
    bool const in_r = map_runtime().east_goal() &&
                      puck_overlaps_goal_zone(puck_x, puck_y, kPuckDrawRadius, *map_runtime().east_goal());
    bool scored_goal = false;
    if (in_l && !in_r) {
        scored_goal = true;
        if (goals_left_side != 255) {
            ++goals_left_side;
        }
    } else if (in_r && !in_l) {
        scored_goal = true;
        if (goals_right_side != 255) {
            ++goals_right_side;
        }
    } else if (in_l && in_r) {
        scored_goal = true;
        if (puck_x <= rink_center_x()) {
            if (goals_left_side != 255) {
                ++goals_left_side;
            }
        } else {
            if (goals_right_side != 255) {
                ++goals_right_side;
            }
        }
    }

    if (scored_goal) {
        result.scored_goal = true;
        std::printf("[zh] Goal — west rim %u / east rim %u\n",
                    static_cast<unsigned>(goals_left_side), static_cast<unsigned>(goals_right_side));
        start_faceoff_countdown(*this, screen_w, screen_h, slot_active);
    }

    for (std::size_t i = 0; i < kRemoteSlots; ++i) {
        remote_mouse_seen[i] = in.slot_mouse_buttons[i];
    }
    host_mouse_prev = local_mouse_bits;

    return result;
}

}  // namespace zh::game

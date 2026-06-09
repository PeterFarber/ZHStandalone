// WAN client local skater: live advance + sent-input replay after snapshot resync.

#include "zh/client/remote_skater_predictor.hpp"

#include "zh/game/constants.hpp"
#include "zh/game/skater_physics.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace {

void apply_keyboard_direct(float &vx,
                           float &vy,
                           bool &has_wp,
                           float &face_x,
                           float &face_y,
                           std::uint8_t move_axes,
                           float accel,
                           float speed_cap,
                           float dt) noexcept {
    if (move_axes == 0U) {
        return;
    }
    float dx = 0.f;
    float dy = 0.f;
    if ((move_axes & 1U) != 0U) {
        dy -= 1.f;
    }
    if ((move_axes & 2U) != 0U) {
        dy += 1.f;
    }
    if ((move_axes & 4U) != 0U) {
        dx -= 1.f;
    }
    if ((move_axes & 8U) != 0U) {
        dx += 1.f;
    }
    float const len = std::hypot(dx, dy);
    if (len <= 1e-3f) {
        return;
    }
    dx /= len;
    dy /= len;
    has_wp = false;
    face_x = dx;
    face_y = dy;
    vx += dx * accel * dt;
    vy += dy * accel * dt;
    float const spd = std::hypot(vx, vy);
    if (spd > speed_cap && spd > 1e-4f) {
        float const s = speed_cap / spd;
        vx *= s;
        vy *= s;
    }
}

}  // namespace

namespace zh::client {

void RemoteSkaterPredictor::reset() noexcept {
    seeded_ = false;
    px_ = py_ = 0.f;
    vx_ = vy_ = 0.f;
    way_x_ = way_y_ = 0.f;
    has_wp_ = false;
    face_x_ = 1.f;
    face_y_ = 0.f;
    follow_ability_axes_ = 0;
    ability_prev_ = 0;
    boost_cd_ticks_ = {};
    boost_overspeed_ticks_ = 0;
    slide_cd_ticks_ = 0;
    slide_decel_ticks_ = 0;
    slide_carry_speed_ = 0.f;
    brake_cd_ticks_ = 0;
    one_timer_ticks_ = 0;
    one_timer_cd_ticks_ = 0;
    shield_ticks_ = 0;
    shield_cd_ticks_ = 0;
    is_goalie_ = false;
    boost_tick_frac_accum_ = 0.f;
    snap_authority_tick_ = 0;
    mouse_buttons_prev_ = 0;
}

void RemoteSkaterPredictor::authority_resync(std::uint8_t const slot,
                                             PackedSnapshot const &nw,
                                             PackedSnapshot const &prv,
                                             float const fixed_dt_sec) noexcept {
    snap_authority_tick_ = nw.server_tick;

    boost_cd_ticks_[0] = static_cast<std::uint32_t>(nw.slot_boost_cd_ticks[slot]);
    boost_cd_ticks_[1] = static_cast<std::uint32_t>(nw.slot_boost_cd2_ticks[slot]);
    one_timer_ticks_ = static_cast<std::uint32_t>(nw.slot_one_timer_ticks[slot]);
    shield_ticks_ = static_cast<std::uint32_t>(nw.slot_shield_ticks[slot]);
    is_goalie_ = ((nw.match_slot_goalie_mask >> slot) & 1U) != 0U;
    boost_tick_frac_accum_ = 0.f;

    px_ = nw.skater_px[slot];
    py_ = nw.skater_py[slot];

    std::uint8_t const wmask = static_cast<std::uint8_t>(1U << slot);
    if ((nw.skater_waypoint_mask & wmask) != 0U) {
        way_x_ = nw.skater_way_x[slot];
        way_y_ = nw.skater_way_y[slot];
        has_wp_ = true;
        zh::game::set_facing_toward(px_, py_, way_x_, way_y_, face_x_, face_y_);
    } else {
        has_wp_ = false;
    }

    // Infer planar velocity from consecutive authority snaps when server_tick advances.
    if (prv.server_tick > 0U && nw.server_tick > prv.server_tick) {
        float const sim_dt = static_cast<float>(nw.server_tick - prv.server_tick) * fixed_dt_sec;
        if (sim_dt > 1e-4f) {
            vx_ = (nw.skater_px[slot] - prv.skater_px[slot]) / sim_dt;
            vy_ = (nw.skater_py[slot] - prv.skater_py[slot]) / sim_dt;
        }
    } else if (!(nw.server_tick == prv.server_tick && prv.server_tick != 0U)) {
        vx_ = 0.f;
        vy_ = 0.f;
    }

    seeded_ = true;
}

void RemoteSkaterPredictor::apply_ack_follow_axes(std::uint8_t const follow_ability_axes) noexcept {
    follow_ability_axes_ = follow_ability_axes;
}

// One replayed input tick (same physics path as live advance, fixed dt).
void RemoteSkaterPredictor::run_hist_step(SentClientInputRecord const &r,
                                          std::uint8_t &abl_prev,
                                          float const fixed_dt_sec) noexcept {
    bool const z_pulse = ((r.ability_axes & zh::kClientAbilityZ) != 0U) &&
                         ((abl_prev & zh::kClientAbilityZ) == 0U);
    bool const x_pulse = ((r.ability_axes & zh::kClientAbilityX) != 0U) &&
                         ((abl_prev & zh::kClientAbilityX) == 0U);
    bool const s_pulse = ((r.ability_axes & zh::kClientAbilitySBrake) != 0U) &&
                         ((abl_prev & zh::kClientAbilitySBrake) == 0U);
    bool const c_pulse = ((r.ability_axes & zh::kClientAbilityCOneTimer) != 0U) &&
                         ((abl_prev & zh::kClientAbilityCOneTimer) == 0U);
    zh::game::try_activate_one_timer(one_timer_ticks_, one_timer_cd_ticks_, c_pulse);

    if (((r.mouse_buttons & zh::kClientMouseRmbClick) != 0U)) {
        float wx = r.mouse_x;
        float wy = r.mouse_y;
        zh::game::clamp_waypoint_to_rink(wx, wy);
        way_x_ = wx;
        way_y_ = wy;
        has_wp_ = true;
        zh::game::set_facing_toward(px_, py_, wx, wy, face_x_, face_y_);
        zh::game::apply_slide_carry_on_new_waypoint(vx_, vy_, face_x_, face_y_,
                                                    zh::game::player_max_speed(is_goalie_),
                                                    slide_carry_speed_);
        slide_decel_ticks_ = 0U;
    }

    zh::game::step_skater_velocity(vx_, vy_, px_, py_, way_x_, way_y_, has_wp_, face_x_, face_y_,
                                   zh::game::player_accel(is_goalie_),
                                   zh::game::player_max_speed(is_goalie_),
                                   zh::game::kSkaterFrictionCoast, zh::game::kWaypointArrivalDistSq,
                                   zh::game::kWaypointArrivalSpeed, fixed_dt_sec, boost_overspeed_ticks_);

    zh::game::try_apply_brake_tap(vx_, vy_, has_wp_, brake_cd_ticks_, s_pulse);

    if (is_goalie_) {
        zh::game::try_activate_goalie_shield(shield_ticks_, shield_cd_ticks_, x_pulse);
    } else {
        zh::game::try_apply_slide_stop(vx_, vy_, has_wp_, slide_carry_speed_, slide_decel_ticks_,
                                      slide_cd_ticks_, boost_overspeed_ticks_, x_pulse);
        zh::game::tick_slide_decel(vx_, vy_, slide_decel_ticks_);
    }

    zh::game::try_apply_boost_impulse(vx_, vy_, face_x_, face_y_, zh::game::player_max_speed(is_goalie_),
                                      boost_cd_ticks_.data(),
                                      zh::game::player_boost_charge_count(is_goalie_),
                                      boost_overspeed_ticks_, z_pulse,
                                      zh::game::kSkaterBoostImpulse);

    apply_keyboard_direct(vx_, vy_, has_wp_, face_x_, face_y_, r.move_axes,
                          zh::game::player_accel(is_goalie_),
                          zh::game::player_max_speed(is_goalie_), fixed_dt_sec);

    zh::game::integrate_skater_motion(px_, py_, vx_, vy_, fixed_dt_sec,
                                      zh::game::player_draw_radius(is_goalie_));

    zh::game::tick_boost_cooldowns(boost_cd_ticks_.data(), zh::game::kGoalieBoostCharges);
    zh::game::tick_slide_cooldown(slide_cd_ticks_);
    zh::game::tick_brake_cooldown(brake_cd_ticks_);
    zh::game::tick_one_timer_window(one_timer_ticks_);
    zh::game::tick_goalie_shield_window(shield_ticks_);
    if (one_timer_cd_ticks_ > 0U) {
        --one_timer_cd_ticks_;
    }
    if (shield_cd_ticks_ > 0U) {
        --shield_cd_ticks_;
    }

    abl_prev = r.ability_axes;
}

void RemoteSkaterPredictor::consume_boost_frac(float const frame_dt_sec,
                                             float const fixed_dt_sec) noexcept {
    float const frac = frame_dt_sec / fixed_dt_sec;
    if (!std::isfinite(frac) || frac <= 0.f) {
        return;
    }
    boost_tick_frac_accum_ += frac;
    while (boost_tick_frac_accum_ >= 1.f) {
        zh::game::tick_boost_cooldowns(boost_cd_ticks_.data(), zh::game::kGoalieBoostCharges);
        zh::game::tick_slide_cooldown(slide_cd_ticks_);
        zh::game::tick_brake_cooldown(brake_cd_ticks_);
        if (one_timer_cd_ticks_ > 0U) {
            --one_timer_cd_ticks_;
        }
        if (shield_cd_ticks_ > 0U) {
            --shield_cd_ticks_;
        }
        zh::game::tick_one_timer_window(one_timer_ticks_);
        zh::game::tick_goalie_shield_window(shield_ticks_);
        boost_tick_frac_accum_ -= 1.f;
    }
}

// Re-sim sent inputs newer than last authority tick; budget caps work per snapshot.
void RemoteSkaterPredictor::replay_from_hist(ClientSentInputRing const &ring,
                                             bool const slot_valid,
                                             float const fixed_dt_sec) noexcept {
    if (!seeded_ || !slot_valid) {
        return;
    }

    boost_tick_frac_accum_ = 0.f;

    std::size_t const total = ring.count();
    if (total == 0U) {
        ability_prev_ = follow_ability_axes_;
        return;
    }

    std::size_t const skip =
        total > zh::game::kClientPredReplayMaxSteps ? (total - zh::game::kClientPredReplayMaxSteps)
                                                    : 0U;

    std::size_t const replay_n = total - skip;
    std::uint32_t const auth_T = snap_authority_tick_;

    if (replay_n == 0U) {
        ability_prev_ = ring.at_ordered(total - 1U).ability_axes;
        return;
    }

    std::array<SentClientInputRecord, zh::game::kClientPredReplayMaxSteps> wave{};
    for (std::size_t i = 0; i < replay_n; ++i) {
        wave[i] = ring.at_ordered(skip + i);
    }

    std::sort(wave.begin(), wave.begin() + replay_n,
              [](SentClientInputRecord const &a, SentClientInputRecord const &b) noexcept {
                  if (a.est_exec_tick != b.est_exec_tick) {
                      return a.est_exec_tick < b.est_exec_tick;
                  }
                  return a.seq < b.seq;
              });

    std::uint8_t abl_prev = follow_ability_axes_;
    if (skip > 0U) {
        abl_prev = ring.at_ordered(skip - 1U).ability_axes;
    }

    std::uint32_t bucket_est = wave[0].est_exec_tick;
    SentClientInputRecord bucket = wave[0];

    std::size_t physics_budget = zh::game::kClientPredReplayPhysicsBudgetPerSnap;

    auto flush_bucket = [&](SentClientInputRecord const &r, std::uint32_t est) noexcept {
        if (est > auth_T) {
            if (physics_budget > 0U) {
                run_hist_step(r, abl_prev, fixed_dt_sec);
                --physics_budget;
            } else {
                abl_prev = r.ability_axes;
            }
        } else {
            abl_prev = r.ability_axes;
        }
    };

    for (std::size_t i = 1; i < replay_n; ++i) {
        if (wave[i].est_exec_tick == bucket_est) {
            bucket = wave[i];
        } else {
            flush_bucket(bucket, bucket_est);
            bucket = wave[i];
            bucket_est = wave[i].est_exec_tick;
        }
    }
    flush_bucket(bucket, bucket_est);
    ability_prev_ = abl_prev;
}

// Live frame: same physics as replay, then soft pull toward latest authority pose.
void RemoteSkaterPredictor::advance(float frame_dt_sec,
                                   float fixed_dt_sec,
                                   float mouse_x,
                                   float mouse_y,
                                   std::uint8_t mouse_buttons,
                                   std::uint8_t move_axes,
                                   std::uint8_t ability_axes,
                                   std::uint8_t my_slot,
                                   PackedSnapshot const &snap_authority_latest) noexcept {
    if (!seeded_) {
        return;
    }

    constexpr float kPredDtClamp = 0.05f;
    float dt = frame_dt_sec;
    if (!std::isfinite(dt) || dt <= 0.f) {
        return;
    }
    dt = std::min(dt, kPredDtClamp);

    if (((mouse_buttons & zh::kClientMouseRmbHeld) != 0U) &&
        ((mouse_buttons_prev_ & zh::kClientMouseRmbHeld) == 0U)) {
        float wx = mouse_x;
        float wy = mouse_y;
        zh::game::clamp_waypoint_to_rink(wx, wy);
        way_x_ = wx;
        way_y_ = wy;
        has_wp_ = true;
        zh::game::set_facing_toward(px_, py_, wx, wy, face_x_, face_y_);
        zh::game::apply_slide_carry_on_new_waypoint(vx_, vy_, face_x_, face_y_,
                                                    zh::game::player_max_speed(is_goalie_),
                                                    slide_carry_speed_);
        slide_decel_ticks_ = 0U;
    }
    mouse_buttons_prev_ = mouse_buttons;

    bool const z_pulse = ((ability_axes & zh::kClientAbilityZ) != 0U) &&
                         ((ability_prev_ & zh::kClientAbilityZ) == 0U);
    bool const x_pulse = ((ability_axes & zh::kClientAbilityX) != 0U) &&
                         ((ability_prev_ & zh::kClientAbilityX) == 0U);
    bool const s_pulse = ((ability_axes & zh::kClientAbilitySBrake) != 0U) &&
                         ((ability_prev_ & zh::kClientAbilitySBrake) == 0U);
    bool const c_pulse = ((ability_axes & zh::kClientAbilityCOneTimer) != 0U) &&
                         ((ability_prev_ & zh::kClientAbilityCOneTimer) == 0U);
    zh::game::try_activate_one_timer(one_timer_ticks_, one_timer_cd_ticks_, c_pulse);

    is_goalie_ = ((snap_authority_latest.match_slot_goalie_mask >> my_slot) & 1U) != 0U;

    zh::game::step_skater_velocity(vx_, vy_, px_, py_, way_x_, way_y_, has_wp_, face_x_, face_y_,
                                   zh::game::player_accel(is_goalie_),
                                   zh::game::player_max_speed(is_goalie_),
                                   zh::game::kSkaterFrictionCoast, zh::game::kWaypointArrivalDistSq,
                                   zh::game::kWaypointArrivalSpeed, dt, boost_overspeed_ticks_);

    zh::game::try_apply_brake_tap(vx_, vy_, has_wp_, brake_cd_ticks_, s_pulse);

    if (is_goalie_) {
        zh::game::try_activate_goalie_shield(shield_ticks_, shield_cd_ticks_, x_pulse);
    } else {
        zh::game::try_apply_slide_stop(vx_, vy_, has_wp_, slide_carry_speed_, slide_decel_ticks_,
                                      slide_cd_ticks_, boost_overspeed_ticks_, x_pulse);
        zh::game::tick_slide_decel(vx_, vy_, slide_decel_ticks_);
    }

    zh::game::try_apply_boost_impulse(vx_, vy_, face_x_, face_y_, zh::game::player_max_speed(is_goalie_),
                                      boost_cd_ticks_.data(),
                                      zh::game::player_boost_charge_count(is_goalie_),
                                      boost_overspeed_ticks_, z_pulse,
                                      zh::game::kSkaterBoostImpulse);

    float const speed_cap =
        (boost_overspeed_ticks_ > 0U)
            ? (zh::game::player_max_speed(is_goalie_) + zh::game::kSkaterBoostOverspeed)
            : zh::game::player_max_speed(is_goalie_);
    apply_keyboard_direct(vx_, vy_, has_wp_, face_x_, face_y_, move_axes,
                          zh::game::player_accel(is_goalie_), speed_cap, dt);

    zh::game::integrate_skater_motion(px_, py_, vx_, vy_, dt,
                                      zh::game::player_draw_radius(is_goalie_));

    consume_boost_frac(dt, fixed_dt_sec);

    // Pull strength scales down when host input delay B is high (avoid fighting delayed ack).
    if (my_slot < zh::kRemoteSlots) {
        float const auth_x = snap_authority_latest.skater_px[my_slot];
        float const auth_y = snap_authority_latest.skater_py[my_slot];

        float const reconcile_b_scale =
            1.f / (1.f + static_cast<float>(snap_authority_latest.input_delay_ticks_b) *
                            zh::game::kClientPredictPullBdampPerTick);
        float const pull_amt =
            std::min(1.f, zh::game::kClientPredictAuthSnapPullPerSec * reconcile_b_scale * dt);
        px_ += (auth_x - px_) * pull_amt;
        py_ += (auth_y - py_) * pull_amt;
    }

    ability_prev_ = ability_axes;
}

float RemoteSkaterPredictor::one_timer_fraction() const noexcept {
    return zh::game::one_timer_fraction(one_timer_ticks_);
}

float RemoteSkaterPredictor::shield_fraction() const noexcept {
    return zh::game::goalie_shield_fraction(shield_ticks_);
}

}  // namespace zh::client

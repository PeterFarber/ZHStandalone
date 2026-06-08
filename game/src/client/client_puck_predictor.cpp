// WAN client puck prediction: steal/charge/shoot heuristics, reconcile toward snapshot authority.

#include "zh/client/client_puck_predictor.hpp"

#include "zh/client/interp_velocity.hpp"
#include "zh/game/constants.hpp"
#include "zh/game/puck_physics.hpp"
#include "zh/game/skater_physics.hpp"

#include <algorithm>
#include <cmath>

namespace zh::client {

namespace {

// When carrier is nearly still, blade axis follows interpolated skater displacement.
void infer_rest_facing(float vx,
                       float vy,
                       float older_px,
                       float older_py,
                       float newer_px,
                       float newer_py,
                       float &face_x,
                       float &face_y) noexcept {
    if (std::hypot(vx, vy) > zh::game::kSkaterMoveFacingSpeed) {
        face_x = 1.f;
        face_y = 0.f;
        return;
    }
    float const dx = newer_px - older_px;
    float const dy = newer_py - older_py;
    float const len = std::hypot(dx, dy);
    if (len < 1e-3f) {
        face_x = 1.f;
        face_y = 0.f;
        return;
    }
    face_x = dx / len;
    face_y = dy / len;
}

void apply_snapshot_goalie_shields_to_puck(float &puck_x,
                                           float &puck_y,
                                           float &puck_vx,
                                           float &puck_vy,
                                           ClientInterpFrame const &r,
                                           PackedSnapshot const &snap) noexcept {
    float const t_v = r.mix_t;

    if (snap.match_host_goalie != 0U && snap.host_shield_ticks > 0U) {
        float const hx =
            r.older_snap.host_px + (r.newer_snap.host_px - r.older_snap.host_px) * t_v;
        float const hy =
            r.older_snap.host_py + (r.newer_snap.host_py - r.older_snap.host_py) * t_v;
        (void)zh::game::try_reflect_loose_puck_off_shield(puck_x, puck_y, puck_vx, puck_vy, hx, hy);
    }

    for (std::size_t i = 0; i < zh::kRemoteSlots; ++i) {
        if ((snap.slot_occupied_mask & static_cast<std::uint8_t>(1U << i)) == 0U) {
            continue;
        }
        if ((snap.match_slot_goalie_mask & static_cast<std::uint8_t>(1U << i)) == 0U) {
            continue;
        }
        if (snap.slot_shield_ticks[i] == 0U) {
            continue;
        }
        float const sx = r.older_snap.skater_px[i] +
                         (r.newer_snap.skater_px[i] - r.older_snap.skater_px[i]) * t_v;
        float const sy = r.older_snap.skater_py[i] +
                         (r.newer_snap.skater_py[i] - r.older_snap.skater_py[i]) * t_v;
        (void)zh::game::try_reflect_loose_puck_off_shield(puck_x, puck_y, puck_vx, puck_vy, sx, sy);
    }
}

}  // namespace

float ClientPuckPredictor::max_charge_sec_for_ticks() noexcept {
    return static_cast<float>(zh::game::kShootChargeMaxTicks) * (1.f / 60.f);
}

void ClientPuckPredictor::reset() noexcept {
    seeded_ = false;
    x_ = y_ = 0.f;
    vx_ = vy_ = 0.f;
    carrier_ = zh::kPuckCarrierFree;
    shoot_charge_ticks_ = 0.f;
    lmb_was_held_ = false;
    steal_cd_ticks_ = 0;
    shoot_cd_ticks_ = 0;
    pickup_block_ticks_ = 0;
    cd_tick_frac_accum_ = 0.f;
}

void ClientPuckPredictor::authority_resync(PackedSnapshot const &nw) noexcept {
    x_ = nw.puck_x;
    y_ = nw.puck_y;
    vx_ = nw.puck_vx;
    vy_ = nw.puck_vy;
    carrier_ = nw.puck_carrier;
    seeded_ = true;
    shoot_charge_ticks_ = 0.f;
}

zh::game::MatchSoundEvents ClientPuckPredictor::advance(float frame_dt_sec,
                                 float fixed_dt_sec,
                                 ClientInterpFrame const &r,
                                 PackedSnapshot const &snap,
                                 std::uint8_t my_slot,
                                 float local_sk_px,
                                 float local_sk_py,
                                 float local_sk_vx,
                                 float local_sk_vy,
                                 float local_sk_face_x,
                                 float local_sk_face_y,
                                 float mouse_x,
                                 float mouse_y,
                                 std::uint8_t mouse_buttons,
                                 std::uint32_t local_one_timer_ticks) noexcept {
    zh::game::MatchSoundEvents sounds{};
    if (!seeded_) {
        return sounds;
    }

    constexpr float kPredDtClamp = 0.05f;
    float dt = frame_dt_sec;
    if (!std::isfinite(dt) || dt <= 0.f) {
        return sounds;
    }
    dt = std::min(dt, kPredDtClamp);

    float const t_v = r.mix_t;

    float const auth_px = snap.puck_x;
    float const auth_py = snap.puck_y;

    float const reconcile_b_scale =
        1.f / (1.f + static_cast<float>(snap.input_delay_ticks_b) * zh::game::kClientPredictPullBdampPerTick);
    float const puck_pull =
        std::min(1.f, zh::game::kClientPredictPuckPullPerSec * reconcile_b_scale * dt);

    float const attach_pull_mul = (carrier_ == zh::kPuckCarrierFree) ? 1.f : 0.42f;

    float const hpx = r.older_snap.host_px + (r.newer_snap.host_px - r.older_snap.host_px) * t_v;
    float const hpy = r.older_snap.host_py + (r.newer_snap.host_py - r.older_snap.host_py) * t_v;

    float carrier_px = 0.f;
    float carrier_py = 0.f;
    float blade_x = x_;
    float blade_y = y_;
    if (carrier_ == zh::kPuckCarrierHost) {
        carrier_px = hpx;
        carrier_py = hpy;
        float hvx = 0.f;
        float hvy = 0.f;
        interp_host_vel(r, hvx, hvy, fixed_dt_sec);
        float hface_x = 1.f;
        float hface_y = 0.f;
        infer_rest_facing(hvx, hvy, r.older_snap.host_px, r.older_snap.host_py, r.newer_snap.host_px,
                          r.newer_snap.host_py, hface_x, hface_y);
        float blade_vx = 0.f;
        float blade_vy = 0.f;
        zh::game::sync_puck_carried(hpx, hpy, hvx, hvy, hface_x, hface_y, blade_x, blade_y, blade_vx,
                                    blade_vy);
    } else if (carrier_ >= 0 && static_cast<std::size_t>(carrier_) < zh::kRemoteSlots) {
        std::size_t const si = static_cast<std::size_t>(carrier_);
        carrier_px = r.older_snap.skater_px[si] +
                     (r.newer_snap.skater_px[si] - r.older_snap.skater_px[si]) * t_v;
        carrier_py = r.older_snap.skater_py[si] +
                     (r.newer_snap.skater_py[si] - r.older_snap.skater_py[si]) * t_v;
        float svx = 0.f;
        float svy = 0.f;
        interp_slot_vel(r, si, svx, svy, fixed_dt_sec);
        float sface_x = 1.f;
        float sface_y = 0.f;
        infer_rest_facing(svx, svy, r.older_snap.skater_px[si], r.older_snap.skater_py[si],
                          r.newer_snap.skater_px[si], r.newer_snap.skater_py[si], sface_x, sface_y);
        float blade_vx = 0.f;
        float blade_vy = 0.f;
        zh::game::sync_puck_carried(carrier_px, carrier_py, svx, svy, sface_x, sface_y, blade_x,
                                    blade_y, blade_vx, blade_vy);
    }

    bool const lmb_held = (mouse_buttons & zh::kClientMouseLmbHeld) != 0U;
    bool const lmb_rising = lmb_held && !lmb_was_held_;

    // Local steal/charge only for joined slot; carrier pose from interpolated peers.
    if (my_slot < zh::kRemoteSlots && lmb_rising && steal_cd_ticks_ == 0U &&
        carrier_ != zh::kPuckCarrierFree &&
        carrier_ != static_cast<std::int8_t>(my_slot)) {
        if (zh::game::skater_can_steal_carried_puck(local_sk_px, local_sk_py, local_sk_vx, local_sk_vy,
                                                    local_sk_face_x, local_sk_face_y, carrier_px,
                                                    carrier_py)) {
            carrier_ = static_cast<std::int8_t>(my_slot);
            vx_ = local_sk_vx;
            vy_ = local_sk_vy;
            x_ = blade_x;
            y_ = blade_y;
            zh::game::apply_puck_strip_nudge(x_, y_, vx_, vy_, mouse_x, mouse_y,
                                             zh::game::kMatchStealDamp, zh::game::kMatchStealNudge);
            steal_cd_ticks_ = zh::game::kPuckStealCooldownTicks;
            shoot_cd_ticks_ = zh::game::kShootAfterStealBlockTicks;
            shoot_charge_ticks_ = 0.f;
        }
    }

    if (my_slot < zh::kRemoteSlots && carrier_ == static_cast<std::int8_t>(my_slot)) {
        if (lmb_held && shoot_cd_ticks_ == 0U) {
            float const charge_dt = std::min(dt, fixed_dt_sec);
            float const max_charge_sec =
                static_cast<float>(zh::game::kShootChargeMaxTicks) * fixed_dt_sec;
            shoot_charge_ticks_ = std::min(max_charge_sec, shoot_charge_ticks_ + charge_dt);
        } else if (lmb_was_held_ && !lmb_held && shoot_charge_ticks_ > 0.f && shoot_cd_ticks_ == 0U) {
            carrier_ = zh::kPuckCarrierFree;
            float const charge_frac =
                std::clamp(shoot_charge_ticks_ / max_charge_sec_for_ticks(), 0.f, 1.f);
            zh::game::apply_charged_puck_shot(local_sk_px, local_sk_py, vx_, vy_, mouse_x, mouse_y,
                                              charge_frac, zh::game::kShootImpulseMin,
                                              zh::game::kShootImpulseMax);
            sounds.shot = true;
            shoot_charge_ticks_ = 0.f;
            pickup_block_ticks_ = zh::game::kPuckSelfPickupBlockTicks;
        }
    } else if (!lmb_held) {
        shoot_charge_ticks_ = 0.f;
    }
    lmb_was_held_ = lmb_held;

    // Per-carrier motion: loose friction+bounce, host/slot follow sync_puck_carried.
    switch (carrier_) {
        case zh::kPuckCarrierFree: {
            float const max_spd_p = 720.f;
            vx_ = std::clamp(vx_, -max_spd_p, max_spd_p);
            vy_ = std::clamp(vy_, -max_spd_p, max_spd_p);
            vx_ *= zh::game::kPuckFloorFriction;
            vy_ *= zh::game::kPuckFloorFriction;
            float const ps = std::hypot(vx_, vy_);
            if (ps < zh::game::kPuckStopSpeed) {
                vx_ = vy_ = 0.f;
            }
            x_ += vx_ * dt;
            y_ += vy_ * dt;
            if (zh::game::resolve_free_puck_against_boards(x_, y_, vx_, vy_)) {
                sounds.puck_collision = true;
            }
            if (zh::game::resolve_free_puck_against_goal_frames(x_, y_, vx_, vy_)) {
                sounds.puck_metal_collision = true;
            }
            apply_snapshot_goalie_shields_to_puck(x_, y_, vx_, vy_, r, snap);

            bool const local_is_goalie =
                my_slot < zh::kRemoteSlots &&
                ((snap.match_slot_goalie_mask >> my_slot) & 1U) != 0U;
            float const local_body_r = zh::game::player_draw_radius(local_is_goalie);

            if (my_slot < zh::kRemoteSlots && local_one_timer_ticks > 0U &&
                zh::game::skater_can_pickup_loose_puck(local_sk_px, local_sk_py, local_sk_vx,
                                                       local_sk_vy, local_sk_face_x, local_sk_face_y,
                                                       x_, y_, local_body_r)) {
                zh::game::apply_one_timer_redirect(local_sk_px, local_sk_py, vx_, vy_, mouse_x,
                                                   mouse_y);
                pickup_block_ticks_ = zh::game::kPuckSelfPickupBlockTicks;
            } else if (my_slot < zh::kRemoteSlots && pickup_block_ticks_ == 0U &&
                       zh::game::skater_can_pickup_loose_puck(local_sk_px, local_sk_py, local_sk_vx,
                                                              local_sk_vy, local_sk_face_x,
                                                              local_sk_face_y, x_, y_, local_body_r)) {
                carrier_ = static_cast<std::int8_t>(my_slot);
            }
            break;
        }
        case zh::kPuckCarrierHost: {
            float hvx = 0.f;
            float hvy = 0.f;
            interp_host_vel(r, hvx, hvy, fixed_dt_sec);
            float hface_x = 1.f;
            float hface_y = 0.f;
            infer_rest_facing(hvx, hvy, r.older_snap.host_px, r.older_snap.host_py, r.newer_snap.host_px,
                              r.newer_snap.host_py, hface_x, hface_y);
            zh::game::sync_puck_carried(hpx, hpy, hvx, hvy, hface_x, hface_y, x_, y_, vx_, vy_);
            break;
        }
        default: {
            std::int8_t const car = carrier_;
            if (my_slot < zh::kRemoteSlots && car == static_cast<std::int8_t>(my_slot)) {
                zh::game::sync_puck_carried(local_sk_px, local_sk_py, local_sk_vx, local_sk_vy,
                                            local_sk_face_x, local_sk_face_y, x_, y_, vx_, vy_);
            } else if (car >= 0 && static_cast<std::size_t>(car) < zh::kRemoteSlots) {
                std::size_t const si = static_cast<std::size_t>(car);
                float const sx =
                    r.older_snap.skater_px[si] +
                    (r.newer_snap.skater_px[si] - r.older_snap.skater_px[si]) * t_v;
                float const sy =
                    r.older_snap.skater_py[si] +
                    (r.newer_snap.skater_py[si] - r.older_snap.skater_py[si]) * t_v;
                float svx = 0.f;
                float svy = 0.f;
                interp_slot_vel(r, si, svx, svy, fixed_dt_sec);
                float sface_x = 1.f;
                float sface_y = 0.f;
                infer_rest_facing(svx, svy, r.older_snap.skater_px[si], r.older_snap.skater_py[si],
                                  r.newer_snap.skater_px[si], r.newer_snap.skater_py[si], sface_x,
                                  sface_y);
                zh::game::sync_puck_carried(sx, sy, svx, svy, sface_x, sface_y, x_, y_, vx_, vy_);
            }
            break;
        }
    }

    // Soft pull toward latest authoritative puck pose; weaker while attached to a carrier.
    x_ += (auth_px - x_) * puck_pull * attach_pull_mul;
    y_ += (auth_py - y_) * puck_pull * attach_pull_mul;

    zh::game::clamp_skater_into_rink(x_, y_, zh::game::kPuckDrawRadius);

    float const cd_frac = dt / fixed_dt_sec;
    if (std::isfinite(cd_frac) && cd_frac > 0.f) {
        cd_tick_frac_accum_ += cd_frac;
        while (cd_tick_frac_accum_ >= 1.f) {
            if (steal_cd_ticks_ > 0U) {
                --steal_cd_ticks_;
            }
            if (shoot_cd_ticks_ > 0U) {
                --shoot_cd_ticks_;
            }
            if (pickup_block_ticks_ > 0U) {
                --pickup_block_ticks_;
            }
            cd_tick_frac_accum_ -= 1.f;
        }
    }

    return sounds;
}

float ClientPuckPredictor::shoot_charge_fraction() const noexcept {
    return std::clamp(shoot_charge_ticks_ / max_charge_sec_for_ticks(), 0.f, 1.f);
}

}  // namespace zh::client

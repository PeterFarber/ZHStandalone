#include "detail/app_playing_view.hpp"
#include "detail/app_context.hpp"

#include "zh/app.hpp"
#include "zh/game/constants.hpp"
#include "zh/game/skater_physics.hpp"
#include "zh/ui/skater_fx.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace zh::detail {

using zh::client::ClientInterpFrame;

namespace {

using namespace zh::game;

[[nodiscard]] float host_render_extrapolate(float const curr, float const prev,
                                          float const alpha) noexcept {
    return curr + (curr - prev) * alpha;
}

void infer_draw_skater_axes(float const older_px,
                            float const older_py,
                            float const newer_px,
                            float const newer_py,
                            float const stored_face_x,
                            float const stored_face_y,
                            float &out_vx,
                            float &out_vy,
                            float &out_face_x,
                            float &out_face_y) noexcept {
    out_face_x = stored_face_x;
    out_face_y = stored_face_y;
    out_vx = (newer_px - older_px) * 60.f;
    out_vy = (newer_py - older_py) * 60.f;
    if (std::hypot(out_vx, out_vy) < 1e-3f) {
        out_vx = stored_face_x * kSkaterMoveFacingSpeed;
        out_vy = stored_face_y * kSkaterMoveFacingSpeed;
    }
    set_facing_toward(older_px, older_py, newer_px, newer_py, out_face_x, out_face_y);
}

}  // namespace

PlayingViewFrame make_playing_view_frame(AppContext const &ctx) noexcept {
    PlayingViewFrame view{};
    view.host_world =
        ctx.listen_server_ != nullptr && ctx.listen_server_->valid() && ctx.match_running_;
    if (ctx.remote_client_) {
        ctx.wan_.snap_hist.sample_interp_frame(GetTime(), ctx.stored_client_interp_delay_ms_,
                                               ctx.snap_new_, ctx.remote_client_, view.interp);
        view.mix_t = view.interp.mix_t;
    }
    return view;
}

void feed_playing_skater_fx(AppContext &ctx, PlayingViewFrame const &view,
                            float const frame_dt) noexcept {
    ClientInterpFrame const &r = view.interp;
    float const t_v = view.mix_t;
    bool const host_world = view.host_world;

    ctx.skater_fx_.begin_frame();

    float hpx = 0.f;
    float hpy = 0.f;
    if (host_world) {
        float const a = view.host_render_alpha;
        hpx = host_render_extrapolate(ctx.world_.host_sk_x, ctx.host_render_prev_sk_x_, a);
        hpy = host_render_extrapolate(ctx.world_.host_sk_y, ctx.host_render_prev_sk_y_, a);
    } else if (ctx.remote_client_) {
        hpx = r.older_snap.host_px + (r.newer_snap.host_px - r.older_snap.host_px) * t_v;
        hpy = r.older_snap.host_py + (r.newer_snap.host_py - r.older_snap.host_py) * t_v;
    }

    if (host_world || ctx.remote_client_) {
        bool host_boost = false;
        bool host_slide = false;
        float hvx_fx = 0.f;
        float hvy_fx = 0.f;
        if (host_world) {
            hvx_fx = ctx.world_.host_sk_vx;
            hvy_fx = ctx.world_.host_sk_vy;
            host_boost = ctx.world_.host_boost_overspeed_ticks > 0U;
            host_slide = ctx.world_.host_slide_decel_ticks > 0U;
        } else {
            float hface_dummy_x = 1.f;
            float hface_dummy_y = 0.f;
            infer_draw_skater_axes(r.older_snap.host_px, r.older_snap.host_py, r.newer_snap.host_px,
                                   r.newer_snap.host_py, 1.f, 0.f, hvx_fx, hvy_fx, hface_dummy_x,
                                   hface_dummy_y);
        }
        ctx.skater_fx_.feed(zh::ui::SkaterFxSlot::Host, hpx, hpy, hvx_fx, hvy_fx, host_boost,
                            host_slide);
    }

    for (std::size_t i = 0; i < kRemoteSlots; ++i) {
        float sx = 0.f;
        float sy = 0.f;
        float svx = 0.f;
        float svy = 0.f;
        bool boost_trail = false;
        bool slide_spray = false;

        if (host_world) {
            if (!ctx.slot_active_[i]) {
                continue;
            }
            sx = ctx.world_.slot_sk_x[i];
            sy = ctx.world_.slot_sk_y[i];
            svx = ctx.world_.slot_sk_vx[i];
            svy = ctx.world_.slot_sk_vy[i];
            boost_trail = ctx.world_.slot_boost_overspeed_ticks[i] > 0U;
            slide_spray = ctx.world_.slot_slide_decel_ticks[i] > 0U;
        } else if (ctx.remote_client_) {
            bool const occ =
                ((r.newer_snap.slot_occupied_mask >> static_cast<unsigned>(i)) & 1U) != 0U;
            if (!occ) {
                continue;
            }
            bool const is_my_here =
                ctx.wan_.my_slot < kRemoteSlots && static_cast<std::size_t>(ctx.wan_.my_slot) == i;
            if (ctx.wan_.skater.seeded() && is_my_here) {
                sx = ctx.wan_.skater.px();
                sy = ctx.wan_.skater.py();
                svx = ctx.wan_.skater.vx();
                svy = ctx.wan_.skater.vy();
                boost_trail = ctx.wan_.skater.boost_overspeed_ticks() > 0U;
                slide_spray = ctx.wan_.skater.slide_decel_ticks() > 0U;
            } else {
                sx = r.older_snap.skater_px[i] +
                     (r.newer_snap.skater_px[i] - r.older_snap.skater_px[i]) * t_v;
                sy = r.older_snap.skater_py[i] +
                     (r.newer_snap.skater_py[i] - r.older_snap.skater_py[i]) * t_v;
                float sface_dummy_x = 1.f;
                float sface_dummy_y = 0.f;
                infer_draw_skater_axes(r.older_snap.skater_px[i], r.older_snap.skater_py[i],
                                       r.newer_snap.skater_px[i], r.newer_snap.skater_py[i], 1.f,
                                       0.f, svx, svy, sface_dummy_x, sface_dummy_y);
            }
        } else {
            continue;
        }

        auto const slot = static_cast<zh::ui::SkaterFxSlot>(static_cast<std::uint8_t>(i) + 1U);
        ctx.skater_fx_.feed(slot, sx, sy, svx, svy, boost_trail, slide_spray);
    }

    ctx.skater_fx_.update(frame_dt);
}

zh::ui::PlayingWorldDraw build_playing_world_draw(AppContext const &ctx,
                                                  PlayingViewFrame const &view) {
    ClientInterpFrame const &r = view.interp;
    float const t_v = view.mix_t;
    bool const host_world = view.host_world;

    std::uint8_t view_slot_team_bmask = 0;
    std::uint8_t view_slot_goalie_mask = 0;
    bool view_host_on_team_b = false;
    bool view_host_is_goalie = false;
    if (host_world) {
        view_slot_team_bmask = ctx.lobby_slot_team_b_mask_;
        view_slot_goalie_mask = ctx.lobby_slot_goalie_mask_;
        view_host_on_team_b = ctx.lobby_host_team_ != 0;
        view_host_is_goalie = ctx.lobby_host_goalie_ != 0;
    } else if (ctx.remote_client_) {
        view_slot_team_bmask = r.newer_snap.match_slot_team_b_mask;
        view_slot_goalie_mask = r.newer_snap.match_slot_goalie_mask;
        view_host_on_team_b = r.newer_snap.match_host_team != 0;
        view_host_is_goalie = r.newer_snap.match_host_goalie != 0;
    }

    float draw_px = 0.f;
    float draw_py = 0.f;
    if (host_world) {
        draw_px = host_render_extrapolate(ctx.world_.puck_x, ctx.host_render_prev_puck_x_,
                                          view.host_render_alpha);
        draw_py = host_render_extrapolate(ctx.world_.puck_y, ctx.host_render_prev_puck_y_,
                                          view.host_render_alpha);
    } else if (ctx.remote_client_) {
        if (ctx.wan_.skater.seeded()) {
            draw_px = ctx.wan_.puck.puck_x();
            draw_py = ctx.wan_.puck.puck_y();
        } else {
            draw_px = r.older_snap.puck_x + (r.newer_snap.puck_x - r.older_snap.puck_x) * t_v;
            draw_py = r.older_snap.puck_y + (r.newer_snap.puck_y - r.older_snap.puck_y) * t_v;
        }
    }

    float hpx = 0.f;
    float hpy = 0.f;
    float hmwx = 0.f;
    float hmwy = 0.f;
    bool draw_host_way = false;
    if (host_world) {
        float const a = view.host_render_alpha;
        hpx = host_render_extrapolate(ctx.world_.host_sk_x, ctx.host_render_prev_sk_x_, a);
        hpy = host_render_extrapolate(ctx.world_.host_sk_y, ctx.host_render_prev_sk_y_, a);
        hmwx = ctx.world_.host_way_x;
        hmwy = ctx.world_.host_way_y;
        draw_host_way = ctx.world_.host_has_waypoint;
    } else if (ctx.remote_client_) {
        hpx = r.older_snap.host_px + (r.newer_snap.host_px - r.older_snap.host_px) * t_v;
        hpy = r.older_snap.host_py + (r.newer_snap.host_py - r.older_snap.host_py) * t_v;

        draw_host_way = r.newer_snap.host_has_waypoint != 0;
        if (r.older_snap.host_has_waypoint != 0 && r.newer_snap.host_has_waypoint != 0) {
            hmwx = r.older_snap.host_way_x + (r.newer_snap.host_way_x - r.older_snap.host_way_x) * t_v;
            hmwy = r.older_snap.host_way_y + (r.newer_snap.host_way_y - r.older_snap.host_way_y) * t_v;
        } else if (r.newer_snap.host_has_waypoint != 0) {
            hmwx = r.newer_snap.host_way_x;
            hmwy = r.newer_snap.host_way_y;
        } else {
            draw_host_way = false;
        }
    }

    zh::ui::PlayingWorldDraw world_draw{};
    world_draw.active = host_world || ctx.remote_client_;
    world_draw.puck_x = draw_px;
    world_draw.puck_y = draw_py;

    if (host_world || ctx.remote_client_) {
        world_draw.host.active = true;
        world_draw.host.is_local = host_world;
        world_draw.host.x = hpx;
        world_draw.host.y = hpy;
        world_draw.host.team_b = view_host_on_team_b;
        world_draw.host.goalie = view_host_is_goalie;
        world_draw.host.has_waypoint = draw_host_way;
        world_draw.host.way_x = hmwx;
        world_draw.host.way_y = hmwy;
        if (host_world && ctx.world_.host_one_timer_ticks > 0U) {
            world_draw.host.one_timer_frac = ctx.world_.host_one_timer_fraction();
        } else if (ctx.remote_client_ && r.newer_snap.host_one_timer_ticks > 0U) {
            world_draw.host.one_timer_frac = one_timer_fraction(
                static_cast<std::uint32_t>(r.newer_snap.host_one_timer_ticks));
        }
        if (host_world && view_host_is_goalie && ctx.world_.host_shield_ticks > 0U) {
            world_draw.host.shield_frac = ctx.world_.host_shield_fraction();
        } else if (ctx.remote_client_ && view_host_is_goalie && r.newer_snap.host_shield_ticks > 0U) {
            world_draw.host.shield_frac = goalie_shield_fraction(
                static_cast<std::uint32_t>(r.newer_snap.host_shield_ticks));
        }
        (void)std::snprintf(world_draw.host.name, sizeof(world_draw.host.name), "Player 1");
    }

    for (std::size_t i = 0; i < kRemoteSlots; ++i) {
        std::uint8_t const mask =
            host_world ? ctx.occupied_slots_mask_ : r.newer_snap.slot_occupied_mask;
        if (((mask >> static_cast<unsigned>(i)) & 1U) == 0U) {
            continue;
        }

        float sx = 0.f;
        float sy = 0.f;
        float wpx = 0.f;
        float wpy = 0.f;
        bool wp = false;

        if (host_world) {
            sx = ctx.world_.slot_sk_x[i];
            sy = ctx.world_.slot_sk_y[i];
            wpx = ctx.world_.slot_way_x[i];
            wpy = ctx.world_.slot_way_y[i];
            wp = ctx.world_.slot_has_waypoint[i];
        } else if (ctx.remote_client_) {
            bool const is_my_here =
                ctx.wan_.my_slot < kRemoteSlots && static_cast<std::size_t>(ctx.wan_.my_slot) == i;
            if (ctx.wan_.skater.seeded() && is_my_here) {
                sx = ctx.wan_.skater.px();
                sy = ctx.wan_.skater.py();
                wpx = ctx.wan_.skater.way_x();
                wpy = ctx.wan_.skater.way_y();
                wp = ctx.wan_.skater.has_waypoint();
            } else {
                sx = r.older_snap.skater_px[i] +
                     (r.newer_snap.skater_px[i] - r.older_snap.skater_px[i]) * t_v;
                sy = r.older_snap.skater_py[i] +
                     (r.newer_snap.skater_py[i] - r.older_snap.skater_py[i]) * t_v;
                std::uint8_t const wm_prev = r.older_snap.skater_waypoint_mask;
                std::uint8_t const wm_new = r.newer_snap.skater_waypoint_mask;
                if (((wm_prev >> static_cast<unsigned>(i)) & 1U) != 0U &&
                    ((wm_new >> static_cast<unsigned>(i)) & 1U) != 0U) {
                    wpx = r.older_snap.skater_way_x[i] +
                          (r.newer_snap.skater_way_x[i] - r.older_snap.skater_way_x[i]) * t_v;
                    wpy = r.older_snap.skater_way_y[i] +
                          (r.newer_snap.skater_way_y[i] - r.older_snap.skater_way_y[i]) * t_v;
                    wp = true;
                } else if (((wm_new >> static_cast<unsigned>(i)) & 1U) != 0U) {
                    wpx = r.newer_snap.skater_way_x[i];
                    wpy = r.newer_snap.skater_way_y[i];
                    wp = true;
                }
            }
        }

        bool const roster_team_b = ((view_slot_team_bmask >> static_cast<unsigned>(i)) & 1U) != 0U;
        bool const roster_goalie = ((view_slot_goalie_mask >> static_cast<unsigned>(i)) & 1U) != 0U;
        bool const is_my_slot = ctx.remote_client_ && ctx.wan_.my_slot < kRemoteSlots &&
                                static_cast<std::size_t>(ctx.wan_.my_slot) == i;

        zh::ui::PlayingEntityDraw &slot_draw = world_draw.slots[i];
        slot_draw.active = true;
        slot_draw.x = sx;
        slot_draw.y = sy;
        slot_draw.team_b = roster_team_b;
        slot_draw.goalie = roster_goalie;
        slot_draw.is_local = is_my_slot;
        slot_draw.has_waypoint = wp;
        slot_draw.way_x = wpx;
        slot_draw.way_y = wpy;

        if (host_world) {
            if (ctx.world_.slot_one_timer_ticks[i] > 0U) {
                slot_draw.one_timer_frac = one_timer_fraction(ctx.world_.slot_one_timer_ticks[i]);
            }
        } else if (ctx.remote_client_) {
            if (is_my_slot && ctx.wan_.skater.seeded()) {
                slot_draw.one_timer_frac = ctx.wan_.skater.one_timer_fraction();
            } else if (r.newer_snap.slot_one_timer_ticks[i] > 0U) {
                slot_draw.one_timer_frac = one_timer_fraction(
                    static_cast<std::uint32_t>(r.newer_snap.slot_one_timer_ticks[i]));
            }
        }

        if (host_world) {
            if (roster_goalie && ctx.world_.slot_shield_ticks[i] > 0U) {
                slot_draw.shield_frac = goalie_shield_fraction(ctx.world_.slot_shield_ticks[i]);
            }
        } else if (ctx.remote_client_) {
            if (is_my_slot && ctx.wan_.skater.seeded() && ctx.wan_.skater.is_goalie()) {
                slot_draw.shield_frac = ctx.wan_.skater.shield_fraction();
            } else if (roster_goalie && r.newer_snap.slot_shield_ticks[i] > 0U) {
                slot_draw.shield_frac = goalie_shield_fraction(
                    static_cast<std::uint32_t>(r.newer_snap.slot_shield_ticks[i]));
            }
        }

        if (is_my_slot) {
            (void)std::snprintf(slot_draw.name, sizeof(slot_draw.name), "You");
        } else {
            (void)std::snprintf(slot_draw.name, sizeof(slot_draw.name), "Player %zu", i + 2U);
        }
    }

    return world_draw;
}

void fill_playing_hud_model(AppContext const &ctx, PlayingViewFrame const &view,
                            zh::ui::PlayingHudModel &hud) noexcept {
    ClientInterpFrame const &r = view.interp;
    bool const host_world = view.host_world;

    hud = {};
    hud.show_match = host_world || ctx.remote_client_;
    if (!hud.show_match) {
        return;
    }

    hud.goals_west = host_world ? static_cast<unsigned>(ctx.world_.goals_left_side)
                                : static_cast<unsigned>(r.newer_snap.goals_left_side);
    hud.goals_east = host_world ? static_cast<unsigned>(ctx.world_.goals_right_side)
                                : static_cast<unsigned>(r.newer_snap.goals_right_side);
    if (host_world) {
        hud.match_phase = static_cast<std::uint8_t>(ctx.world_.phase);
        hud.match_period = ctx.world_.period;
        hud.period_ticks_remaining = static_cast<std::uint16_t>(
            std::min(ctx.world_.period_ticks_remaining, static_cast<std::uint32_t>(65535U)));
        hud.faceoff_countdown_ticks = static_cast<std::uint8_t>(
            std::min(ctx.world_.faceoff_countdown_ticks, static_cast<std::uint32_t>(255U)));
    } else if (ctx.remote_client_) {
        hud.match_phase = r.newer_snap.match_phase;
        hud.match_period = r.newer_snap.match_period;
        hud.period_ticks_remaining = r.newer_snap.period_ticks_remaining;
        hud.faceoff_countdown_ticks = r.newer_snap.faceoff_countdown_ticks;
    }

    hud.local_active = true;
    if (host_world) {
        hud.local_goalie = (ctx.world_.match_host_goalie & 1U) != 0U;
        hud.boost_ready = (ctx.world_.host_boost_cd_ticks[0] == 0U) ? 1U : 0U;
        if (hud.local_goalie && ctx.world_.host_boost_cd_ticks[1] == 0U) {
            ++hud.boost_ready;
        }
        hud.boost_max = player_boost_charge_count(hud.local_goalie);
        std::uint32_t longest_cd = ctx.world_.host_boost_cd_ticks[0];
        if (hud.local_goalie && ctx.world_.host_boost_cd_ticks[1] > longest_cd) {
            longest_cd = ctx.world_.host_boost_cd_ticks[1];
        }
        hud.boost_recharge_sec = static_cast<float>(longest_cd) * App::kFixedDt;
        if (hud.boost_ready >= hud.boost_max) {
            hud.boost_recharge_sec = 0.f;
        }
        hud.one_timer_cd_sec = static_cast<float>(ctx.world_.host_one_timer_cd_ticks) * App::kFixedDt;
        hud.one_timer_active = ctx.world_.host_one_timer_ticks > 0U;
        hud.shield_active_frac = ctx.world_.host_shield_fraction();
    } else if (ctx.remote_client_ && ctx.wan_.my_slot < kRemoteSlots) {
        unsigned const slot_ix = static_cast<unsigned>(ctx.wan_.my_slot);
        hud.local_goalie = ((ctx.snap_new_.match_slot_goalie_mask >> slot_ix) & 1U) != 0U;
        hud.boost_max = player_boost_charge_count(hud.local_goalie);
        hud.boost_ready = (ctx.snap_new_.slot_boost_cd_ticks[slot_ix] == 0U) ? 1U : 0U;
        if (hud.local_goalie && ctx.snap_new_.slot_boost_cd2_ticks[slot_ix] == 0U) {
            ++hud.boost_ready;
        }
        std::uint32_t longest_cd = ctx.snap_new_.slot_boost_cd_ticks[slot_ix];
        if (hud.local_goalie && ctx.snap_new_.slot_boost_cd2_ticks[slot_ix] > longest_cd) {
            longest_cd = ctx.snap_new_.slot_boost_cd2_ticks[slot_ix];
        }
        hud.boost_recharge_sec = static_cast<float>(longest_cd) * App::kFixedDt;
        if (hud.boost_ready >= hud.boost_max) {
            hud.boost_recharge_sec = 0.f;
        }
        if (ctx.wan_.skater.seeded()) {
            hud.one_timer_cd_sec =
                static_cast<float>(ctx.wan_.skater.one_timer_cd_ticks()) * App::kFixedDt;
            hud.one_timer_active = ctx.wan_.skater.one_timer_ticks() > 0U;
            hud.shield_active_frac = ctx.wan_.skater.shield_fraction();
        }
    } else {
        hud.local_active = false;
    }

    hud.z_down = IsKeyDown(KEY_Z);
    hud.x_down = IsKeyDown(KEY_X);
    hud.c_down = IsKeyDown(KEY_C);
    hud.s_down = IsKeyDown(KEY_S);
    hud.r_down = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    hud.lmb_shoot = IsMouseButtonDown(MOUSE_BUTTON_LEFT);

    if (host_world && ctx.world_.puck_carrier == kPuckCarrierHost &&
        (ctx.world_.host_shoot_charge_ticks > 0U || hud.lmb_shoot)) {
        hud.show_shoot_bar = true;
        hud.shoot_charge = ctx.world_.host_shoot_charge_fraction();
    } else if (ctx.remote_client_ && ctx.wan_.my_slot < kRemoteSlots && ctx.wan_.skater.seeded() &&
               ctx.wan_.puck.carrier() == static_cast<std::int8_t>(ctx.wan_.my_slot) &&
               (ctx.wan_.puck.shoot_charging() || hud.lmb_shoot)) {
        hud.show_shoot_bar = true;
        hud.shoot_charge = ctx.wan_.puck.shoot_charge_fraction();
    }
}

void draw_faceoff_countdown_overlay(int const countdown_secs) noexcept {
    float const sw = static_cast<float>(GetScreenWidth());
    float const sh = static_cast<float>(GetScreenHeight());
    Vector2 const center{sw * 0.5f, sh * 0.5f};

    char label[8];
    std::snprintf(label, sizeof(label), "%d", std::max(1, countdown_secs));
    int const fs = 112;
    int const tw = MeasureText(label, fs);
    float const circle_r = std::max(88.f, static_cast<float>(tw) * 0.72f + 48.f);

    DrawCircleV(center, circle_r + 6.f, Color{8, 10, 16, 140});
    DrawCircleV(center, circle_r, Color{18, 22, 32, 210});
    DrawRing(center, circle_r - 5.f, circle_r, 0.f, 360.f, 72, Color{255, 220, 120, 255});
    DrawRing(center, circle_r - 9.f, circle_r - 6.f, 0.f, 360.f, 72, Color{255, 220, 120, 90});

    DrawText(label, static_cast<int>(center.x - static_cast<float>(tw) * 0.5f),
             static_cast<int>(center.y - static_cast<float>(fs) * 0.5f), fs, GOLD);
}

}  // namespace zh::detail

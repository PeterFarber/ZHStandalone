// Playing-screen Camera2D: zoom wheel, smooth follow, host vs client focus targets.

#include "detail/app_context.hpp"

#include "zh/game/match_world.hpp"
#include "zh/game/playing_camera.hpp"
#include "zh/game/playing_camera_3d.hpp"
#include "zh/game/rink_layout.hpp"
#include "zh/ui/world_space_3d.hpp"
#include "zh/ui/playing_scene_pick.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace zh {

namespace {

using namespace ::zh::game;

void sync_playing_camera_zoom(Camera2D &cam, float &view_frac, float const view_w, float const view_h) noexcept {
    RectF const play = rink_ice_bounds();
    view_frac = clamp_playing_view_fraction(view_frac);
    cam.zoom = playing_zoom_for_view_fraction(view_frac, view_w, view_h, play);
}

}  // namespace

void detail::AppContext::apply_playing_camera_zoom() noexcept {
    if (!playing_cam_ready_) {
        return;
    }
    float const wheel = GetMouseWheelMove();
    if (wheel == 0.f) {
        return;
    }

    playing_cam_view_frac_ = clamp_playing_view_fraction(
        playing_cam_view_frac_ * std::pow(kPlayingViewFractionWheelStep, -wheel));

    float const view_w = static_cast<float>(GetScreenWidth());
    float const view_h = static_cast<float>(GetScreenHeight());
    sync_playing_camera_zoom(playing_cam_, playing_cam_view_frac_, view_w, view_h);

    clamp_rink_camera_target(playing_cam_smooth_x_, playing_cam_smooth_y_, view_w, view_h,
                             playing_cam_.zoom);
    playing_cam_.target = {playing_cam_smooth_x_, playing_cam_smooth_y_};
    playing_cam_3d_ = make_playing_camera_3d(playing_cam_smooth_x_, playing_cam_smooth_y_,
                                             playing_cam_view_frac_, view_w, view_h,
                                             rink_ice_bounds());
    playing_cam_3d_ready_ = true;
}

void detail::AppContext::reset_playing_camera() noexcept {
    if (world_.phase == MatchPhase::FaceoffCountdown) {
        playing_cam_focus_x_ = world_.puck_x;
        playing_cam_focus_y_ = world_.puck_y;
        playing_cam_smooth_x_ = world_.puck_x;
        playing_cam_smooth_y_ = world_.puck_y;
    } else {
        playing_cam_focus_x_ = rink_center_x();
        playing_cam_focus_y_ = rink_mid_y();
        playing_cam_smooth_x_ = rink_center_x();
        playing_cam_smooth_y_ = rink_mid_y();
    }
    playing_last_aim_x_ = playing_cam_smooth_x_;
    playing_last_aim_y_ = playing_cam_smooth_y_;
    playing_cam_view_frac_ = kPlayingDefaultViewFraction;
    playing_cam_ = Camera2D{};
    float const view_w = static_cast<float>(GetScreenWidth());
    float const view_h = static_cast<float>(GetScreenHeight());
    playing_cam_.offset = {view_w * 0.5f, view_h * 0.5f};
    sync_playing_camera_zoom(playing_cam_, playing_cam_view_frac_, view_w, view_h);
    playing_cam_.target = {playing_cam_smooth_x_, playing_cam_smooth_y_};
    playing_cam_.rotation = 0.f;
    float const view_w_init = static_cast<float>(GetScreenWidth());
    float const view_h_init = static_cast<float>(GetScreenHeight());
    playing_cam_3d_ = make_playing_camera_3d(playing_cam_smooth_x_, playing_cam_smooth_y_,
                                             playing_cam_view_frac_, view_w_init, view_h_init,
                                             rink_ice_bounds());
    playing_cam_3d_ready_ = true;
    playing_cam_ready_ = true;
    playing_rmb_prev_held_ = 0;
    skater_fx_.reset();
}

// Who the camera tracks: host skater/puck, or client predictor / snapshot slot.
void detail::AppContext::resolve_playing_camera_focus(float &focus_x, float &focus_y,
                                                      float const host_render_alpha) const noexcept {
    focus_x = rink_center_x();
    focus_y = rink_mid_y();

    if (listen_server_ != nullptr && listen_server_->valid() && match_running_) {
        if (world_.phase == MatchPhase::FaceoffCountdown) {
            focus_x = world_.puck_x +
                      (world_.puck_x - host_render_prev_puck_x_) * host_render_alpha;
            focus_y = world_.puck_y +
                      (world_.puck_y - host_render_prev_puck_y_) * host_render_alpha;
        } else {
            focus_x = world_.host_sk_x +
                      (world_.host_sk_x - host_render_prev_sk_x_) * host_render_alpha;
            focus_y = world_.host_sk_y +
                      (world_.host_sk_y - host_render_prev_sk_y_) * host_render_alpha;
        }
        return;
    }

    if (!remote_client_) {
        return;
    }

    if (snap_new_.match_phase == static_cast<std::uint8_t>(MatchPhase::FaceoffCountdown)) {
        focus_x = snap_new_.puck_x;
        focus_y = snap_new_.puck_y;
        return;
    }

    if (wan_.skater.seeded()) {
        focus_x = wan_.skater.px();
        focus_y = wan_.skater.py();
        return;
    }

    if (wan_.my_slot < kRemoteSlots) {
        focus_x = snap_new_.skater_px[wan_.my_slot];
        focus_y = snap_new_.skater_py[wan_.my_slot];
    }
}

void detail::AppContext::update_playing_camera(float const focus_x,
                                               float const focus_y,
                                               float const frame_dt) noexcept {
    if (!playing_cam_ready_) {
        reset_playing_camera();
    }

    playing_cam_focus_x_ = focus_x;
    playing_cam_focus_y_ = focus_y;

    float const dt = std::clamp(frame_dt, 0.f, 0.05f);
    float const alpha = 1.f - std::exp(-9.f * dt);
    playing_cam_smooth_x_ += (focus_x - playing_cam_smooth_x_) * alpha;
    playing_cam_smooth_y_ += (focus_y - playing_cam_smooth_y_) * alpha;

    float const view_w = static_cast<float>(GetScreenWidth());
    float const view_h = static_cast<float>(GetScreenHeight());
    sync_playing_camera_zoom(playing_cam_, playing_cam_view_frac_, view_w, view_h);
    clamp_rink_camera_target(playing_cam_smooth_x_, playing_cam_smooth_y_, view_w, view_h,
                             playing_cam_.zoom);

    playing_cam_.offset = {view_w * 0.5f, view_h * 0.5f};
    playing_cam_.target = {playing_cam_smooth_x_, playing_cam_smooth_y_};
    playing_cam_3d_ = make_playing_camera_3d(playing_cam_smooth_x_, playing_cam_smooth_y_,
                                             playing_cam_view_frac_, view_w, view_h,
                                             rink_ice_bounds());
    playing_cam_3d_ready_ = true;
}

bool detail::AppContext::playing_screen_to_world(Vector2 const screen,
                                                Vector2 &out_sim) const noexcept {
    if (!playing_cam_3d_ready_) {
        if (!playing_cam_ready_) {
            out_sim = screen;
            return false;
        }
        out_sim = GetScreenToWorld2D(screen, playing_cam_);
        return true;
    }
    return zh::ui::screen_to_sim_ground(screen, playing_cam_3d_, out_sim);
}

Vector2 detail::AppContext::playing_screen_to_world(Vector2 const screen) const noexcept {
    Vector2 sim{playing_last_aim_x_, playing_last_aim_y_};
    (void)playing_screen_to_world(screen, sim);
    return sim;
}

void detail::AppContext::refresh_playing_pick_debug(Vector2 const screen,
                                                    bool const log_terminal,
                                                    char const *button) noexcept {
    Vector2 sim{};
    bool const ok = playing_screen_to_world(screen, sim);
    playing_pick_debug_sim_ = sim;
    playing_pick_debug_ok_ = ok;

    playing_pick_debug_pick_ = zh::ui::PlayingPickResult{};
    if (playing_cam_3d_ready_ && playing_debug_world_draw_valid_) {
        playing_pick_debug_pick_ =
            zh::ui::pick_playing_scene(screen, playing_cam_3d_, playing_debug_world_draw_);
    }

    if (log_terminal) {
        zh::ui::log_playing_pick_to_stderr(
            button, playing_pick_debug_pick_, screen, sim, ok, playing_last_aim_x_,
            playing_last_aim_y_);
    }
}

void detail::AppContext::draw_playing_pick_debug_overlay() noexcept {
    if (IsKeyPressed(KEY_F3)) {
        playing_pick_debug_ = !playing_pick_debug_;
        std::fprintf(stderr, "[zh][pick] debug %s (F3 overlay + lines, terminal logs on click)\n",
                     playing_pick_debug_ ? "ON" : "OFF");
    }

    if (!playing_pick_debug_) {
        return;
    }

    Vector2 const mp = GetMousePosition();
    refresh_playing_pick_debug(mp, false, nullptr);
    if (playing_cam_3d_ready_) {
        zh::ui::draw_playing_pick_debug(playing_cam_3d_, mp, playing_pick_debug_sim_,
                                        playing_pick_debug_ok_, playing_last_aim_x_,
                                        playing_last_aim_y_, playing_pick_debug_pick_);
    } else {
        DrawText("F3 pick debug: 3D camera not ready", 12, 84, 14, BLACK);
    }
}

}  // namespace zh

// Playing-screen Camera2D: zoom wheel, smooth follow, host vs client focus targets.

#include "detail/app_context.hpp"

#include "zh/game/match_world.hpp"
#include "zh/game/playing_camera.hpp"
#include "zh/game/rink_layout.hpp"

#include <algorithm>
#include <cmath>

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
    playing_cam_view_frac_ = kPlayingMaxVisiblePlayFraction;
    playing_cam_ = Camera2D{};
    float const view_w = static_cast<float>(GetScreenWidth());
    float const view_h = static_cast<float>(GetScreenHeight());
    playing_cam_.offset = {view_w * 0.5f, view_h * 0.5f};
    sync_playing_camera_zoom(playing_cam_, playing_cam_view_frac_, view_w, view_h);
    playing_cam_.target = {playing_cam_smooth_x_, playing_cam_smooth_y_};
    playing_cam_.rotation = 0.f;
    playing_cam_ready_ = true;
    skater_fx_.reset();
}

// Who the camera tracks: host skater/puck, or client predictor / snapshot slot.
void detail::AppContext::resolve_playing_camera_focus(float &focus_x, float &focus_y) const noexcept {
    focus_x = rink_center_x();
    focus_y = rink_mid_y();

    if (listen_server_ != nullptr && listen_server_->valid() && match_running_) {
        if (world_.phase == MatchPhase::FaceoffCountdown) {
            focus_x = world_.puck_x;
            focus_y = world_.puck_y;
        } else {
            focus_x = world_.host_sk_x;
            focus_y = world_.host_sk_y;
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
}

Vector2 detail::AppContext::playing_screen_to_world(Vector2 const screen) const noexcept {
    if (!playing_cam_ready_) {
        return screen;
    }
    return GetScreenToWorld2D(screen, playing_cam_);
}

}  // namespace zh

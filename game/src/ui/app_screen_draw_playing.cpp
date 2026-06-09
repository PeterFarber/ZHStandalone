// Playing screen: camera, 3D scene, HUD, pick-debug overlay.

#include "detail/app_context.hpp"
#include "detail/app_playing_view.hpp"

#include "zh/gfx/gfx_compat.hpp"
#include "zh/game/rink_layout.hpp"
#include "zh/ui/hit_test.hpp"
#include "zh/ui/playing_hud.hpp"

namespace zh::detail {

using namespace zh::game;

namespace {

using zh::ui::point_in;

}  // namespace

void AppContext::draw_playing(float const interp_alpha, float const frame_dt) {
    PlayingViewFrame view = make_playing_view_frame(*this);
    view.host_render_alpha = interp_alpha;

    float cam_focus_x = rink_center_x();
    float cam_focus_y = rink_mid_y();
    resolve_playing_camera_focus(cam_focus_x, cam_focus_y, interp_alpha);
    if (remote_client_ &&
        view.interp.newer_snap.match_phase ==
            static_cast<std::uint8_t>(MatchPhase::FaceoffCountdown)) {
        cam_focus_x = view.interp.older_snap.puck_x +
                        (view.interp.newer_snap.puck_x - view.interp.older_snap.puck_x) *
                            view.mix_t;
        cam_focus_y = view.interp.older_snap.puck_y +
                        (view.interp.newer_snap.puck_y - view.interp.older_snap.puck_y) *
                            view.mix_t;
    }
    update_playing_camera(cam_focus_x, cam_focus_y, frame_dt);

    DrawPlayingSkyGradient();

    feed_playing_skater_fx(*this, view, frame_dt);

    zh::ui::PlayingWorldDraw const world_draw = build_playing_world_draw(*this, view);
    playing_debug_world_draw_ = world_draw;
    playing_debug_world_draw_valid_ = world_draw.active;

    playing_scene_3d_.ensure_ready();
    playing_scene_3d_.draw_frame(playing_cam_3d_, world_draw, &skater_fx_);

    zh::ui::PlayingHudModel hud{};
    fill_playing_hud_model(*this, view, hud);

    if (view.host_world && host_session_hint_[0] != '\0' &&
        GetTime() < host_session_hint_until_sec_) {
        DrawText(host_session_hint_, 58, static_cast<int>(GetScreenHeight()) - 110, 18,
                 Color{240, 220, 150, 255});
    }

    if (hud.show_match &&
        hud.match_phase == static_cast<std::uint8_t>(MatchPhase::FaceoffCountdown) &&
        hud.faceoff_countdown_ticks > 0U) {
        int const countdown_secs = faceoff_countdown_secs_from_ticks(
            static_cast<std::uint32_t>(hud.faceoff_countdown_ticks));
        draw_faceoff_countdown_overlay(countdown_secs);
    }

    zh::ui::PlayingHudLayout const hud_layout = zh::ui::draw_playing_hud(hud);

    draw_playing_pick_debug_overlay();

    Vector2 const mp = GetMousePosition();
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (point_in(hud_layout.menu_button, mp)) {
            stop_client();
            stop_listen_server();
            client_reset_session();
            host_reset_session();
            screen_ = Screen::Home;
        } else if (point_in(hud_layout.settings_button, mp)) {
            screen_ = Screen::Options;
        }
    }
}

}  // namespace zh::detail

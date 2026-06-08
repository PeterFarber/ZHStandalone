// Fixed-timestep sim dispatch and per-screen rendering.
#include "detail/app_context.hpp"

namespace zh {

using zh::detail::Screen;

void detail::AppContext::sim_fixed_step(float dt) {
    switch (screen_) {
        case Screen::Home:
            sim_home(dt);
            break;
        case Screen::Playing:
            sim_match(dt);  // host only does real work; client Playing sim is a no-op
            break;
        default:
            break;
    }
}

void detail::AppContext::sim_home(float /*dt*/) {}

void detail::AppContext::render_frame(float interp_alpha, float frame_dt) {
    BeginDrawing();
    ClearBackground(Color{15, 20, 32, 255});

    switch (screen_) {
        case Screen::Home:
            draw_home();
            break;
        case Screen::JoinForm:
            draw_join_form();
            break;
        case Screen::HostLobby:
            draw_host_lobby();
            break;
        case Screen::ClientLobby:
            draw_client_lobby();
            break;
        case Screen::Options:
            draw_options();
            break;
        case Screen::Playing:
            apply_playing_camera_zoom();
            if (remote_client_) {
                // Run before draw so local skater/puck feel responsive on WAN.
                advance_client_local_predictor(frame_dt);
                advance_client_puck_prediction(frame_dt);
            }
            poll_client_local_sounds();
            poll_match_ambient_sounds();
            draw_playing(interp_alpha, frame_dt);
            break;
    }

    DrawFPS(GetScreenWidth() - 118, GetScreenHeight() - 28);
    EndDrawing();
}

}  // namespace zh

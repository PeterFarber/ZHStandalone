// Window setup and the main loop: network poll → fixed sim steps → draw.
#include "detail/app_context.hpp"
#include "detail/resource_paths.hpp"

#include "zh/game/map_io.hpp"
#include "zh/game/map_runtime.hpp"
#include "zh/game/rink_layout.hpp"

#include "zh/app.hpp"
#include "zh/audio/audio_engine.hpp"
#include "zh/gfx/gfx_compat.hpp"

#include <cstdio>

#include "zh/ui/render_test_scene.hpp"

namespace zh {

using namespace ::zh::game;

int detail::AppContext::run(int argc, char **argv) {
#if defined(_WIN32)
    // High DPI here desyncs mouse coords from GetScreenWidth/Height on some setups.
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
#else
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE |
                   FLAG_MSAA_4X_HINT);
#endif
    InitWindow(kScreenW, kScreenH, "ZH Standalone");
    SetWindowMinSize(kDesignScreenW / 2, kDesignScreenH / 2);

    ensure_resource_cwd();
    if (!map_runtime().load_from_file(default_map_path().c_str())) {
        map_runtime().reset_to_baked_default();
    }
    load_persisted_user_settings();
    load_rink_texture();
    if (!zh::audio::global_engine().init()) {
        std::fprintf(stderr, "[zh] Warning: audio engine failed to start\n");
    }
    load_game_sound_bank();

    if (zh::ui::boot_render_test_scene(argc, argv)) {
        screen_ = Screen::RenderTest;
        render_test_.reset_camera();
    }

    SetTargetFPS(60);
    PollInputEvents();

    while (!WindowShouldClose()) {
        PollInputEvents();

        float const frame_dt = GetFrameTime();
        if (screen_ == Screen::Playing) {
            apply_playing_camera_zoom();
            refresh_local_playing_input(frame_dt);
        } else {
            invalidate_local_playing_input_cache();
        }

        poll_network();
        sim_accumulator_ += frame_dt;

        constexpr int kMaxCatchUp = 5;
        int caught = 0;
        while (sim_accumulator_ >= App::kFixedDt && caught < kMaxCatchUp) {
            sim_fixed_step(App::kFixedDt);
            sim_accumulator_ -= App::kFixedDt;
            caught++;
        }
        // alpha reserved for render interpolation; playing HUD mostly uses predictors today.
        float const alpha =
            (caught >= kMaxCatchUp) ? 0.f : (sim_accumulator_ / App::kFixedDt);
        render_frame(alpha, frame_dt);
        invalidate_local_playing_input_cache();
    }

    stop_listen_server();
    stop_client();
    unload_rink_texture_if_any();
    unload_game_sound_bank_if_any();
    zh::audio::global_engine().shutdown();
    client_reset_session();
    host_reset_session();
    net::shutdown_library();
    CloseWindow();
    return 0;
}

}  // namespace zh

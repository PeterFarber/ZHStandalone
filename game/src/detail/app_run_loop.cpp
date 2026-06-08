// Window setup and the main loop: network poll → fixed sim steps → draw.
#include "detail/app_context.hpp"
#include "detail/resource_paths.hpp"

#include "zh/game/map_io.hpp"
#include "zh/game/map_runtime.hpp"

#include "zh/app.hpp"

#include <raylib.h>

#include "zh/game/constants.hpp"

namespace zh {

using namespace ::zh::game;

int detail::AppContext::run() {
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
    load_player_team_textures();
    load_rink_texture();
    InitAudioDevice();
    load_game_sound_bank();

    // Raylib polls at EndDrawing(). One PollInputEvents() here seeds state; don't repeat each frame.
    SetTargetFPS(60);
    PollInputEvents();

    while (!WindowShouldClose()) {
        poll_network();

        float const frame_dt = GetFrameTime();
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
    }

    stop_listen_server();
    stop_client();
    unload_player_team_textures_if_any();
    unload_rink_texture_if_any();
    unload_game_sound_bank_if_any();
    if (IsAudioDeviceReady()) {
        CloseAudioDevice();
    }
    client_reset_session();
    host_reset_session();
    net::shutdown_library();
    CloseWindow();
    return 0;
}

}  // namespace zh

# detail/ — application layer

`AppContext` holds session state, screens, net glue, and render assets. `zh::App` in `src/app.cpp` is just a pointer to it.

Frame loop (`app_run_loop.cpp`):

```
poll_network → sim_fixed_step (60 Hz, capped catch-up) → render_frame
```

| File | What it does |
|------|----------------|
| `app_run_loop.cpp` | Window init, main loop, shutdown |
| `app_frame_sim_render.cpp` | Route sim/render by screen |
| `app_input_sampling.cpp` | Keys, mouse, camera while playing |
| `app_net_transport.cpp` | Start/stop host and client |
| `app_session_lobby.cpp` | Lobby teams, match start, spawns |
| `app_sim_match.cpp` | Host tick → MatchWorld → snapshot |
| `app_wan_poll.cpp` | Client predictors + receive |
| `host_listen_poll.cpp` | Host ENet → router |
| `app_user_settings.cpp` | Load/save options JSON |
| `app_playing_camera.cpp` | Zoom/follow cam |
| `app_player_sprite_textures.cpp` | Team player PNGs |
| `app_rink_texture.cpp` | Background / rink art |

Drawing: `src/ui/app_screen_draw.cpp`.

Public onboarding: `docs/code-tour.md`.

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
| `app_playing_camera.cpp` | Zoom/follow cam, F3 pick-debug overlay |
| `app_playing_view.cpp` | Playing FX feed, world/HUD model assembly |
| `app_rink_texture.cpp` | Playing-scene init (`PlayingScene3D::ensure_ready`) |

Drawing:

| File | What it draws |
|------|----------------|
| `src/ui/app_screen_draw.cpp` | Home, lobby, options, render test |
| `src/ui/app_screen_draw_playing.cpp` | Playing screen (thin orchestration) |
| `src/ui/playing_hud.cpp` | Match HUD, ability bar, shoot meter |
| `src/ui/playing_scene_3d.cpp` | Rink, entities, 3D scene |

Public onboarding: `docs/code-tour.md`.

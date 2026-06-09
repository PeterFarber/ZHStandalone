# Code tour

Quick map of where things live. Read [project-overview.md](project-overview.md) first.

## Entry and main loop

| File | Role |
|------|------|
| `game/src/main.cpp` | `main()` → `zh::App::run()` |
| `game/src/app.cpp` | Thin facade; real state in `AppContext` |
| `game/src/detail/app_run_loop.cpp` | Window, map load, **poll → sim → render** loop |
| `game/src/detail/app_frame_sim_render.cpp` | Dispatches sim/render by screen |
| `game/src/detail/README.md` | Lists every `detail/*.cpp` file |

Each frame (simplified):

```
poll_network()
while (sim time due) sim_fixed_step()   // Playing → sim_match on host
render_frame()
```

## UI screens

Screen drawing is split by screen:

| File | Screens |
|------|---------|
| **`game/src/ui/app_screen_draw.cpp`** | Home, lobby, options, render test |
| **`game/src/ui/app_screen_draw_playing.cpp`** | Playing (camera → 3D scene → HUD) |
| **`game/src/detail/app_playing_view.cpp`** | Builds playing world/HUD models from sim + net state |
| **`game/src/ui/playing_hud.cpp`** | Match HUD layout and widgets |
| **`game/src/ui/playing_scene_3d.cpp`** | Rink and entity draw |

Screen enum: **`game/src/detail/app_context.hpp`** (`Screen::Home`, `Playing`, …).

Input sampling while playing: **`app_input_sampling.cpp`**.

## Host (listen server)

| File | Role |
|------|------|
| `game/src/network.cpp` | ENet `ListenHost` / `GameClient` wrappers |
| `game/src/detail/host_listen_poll.cpp` | Host ENet events each frame |
| `game/src/net/host/host_gameplay_router.cpp` | Connect/disconnect, ingest client inputs |
| `game/src/detail/app_sim_match.cpp` | One host tick: delay queue → `MatchWorld` → snapshot |
| `game/src/net/host/host_snapshot_broadcast.cpp` | Send `PackedSnapshot` to clients |
| `game/src/detail/app_session_lobby.cpp` | Lobby teams, start match, spawns |

## Authoritative simulation

| File | Role |
|------|------|
| `game/include/zh/game/match_world.hpp` | Match state struct |
| `game/src/game/match_world.cpp` | Host tick orchestration |
| `game/src/game/skater_physics.cpp` | Movement, boost, slide, brake |
| `game/src/game/puck_physics.cpp` | Puck, shots, carrier |
| `game/include/zh/game/constants.hpp` | Speeds, cooldowns, period length |
| `game/src/game/map_io.cpp` | Load/save map JSON |
| `game/src/game/map_runtime.cpp` | Colliders, goals, spawns at runtime |

No gfx headers in `include/zh/game/` — keep domain code portable.

## WAN client

| File | Role |
|------|------|
| `game/include/zh/client/wan_client_session.hpp` | Client session API |
| `game/src/client/wan_client_session.cpp` | Receive snapshots, send inputs |
| `game/src/client/remote_skater_predictor.cpp` | Local skater prediction |
| `game/src/client/client_puck_predictor.cpp` | Local puck preview |
| `game/src/detail/app_wan_poll.cpp` | Wires predictors + `poll_receive` |

Extend **`WanClientSession`** for new client-side net state instead of growing `AppContext` when you can.

## Wire format

**`game/include/zh/protocol.hpp`** — `PackedClientInput`, `PackedSnapshot`, `PackedLobbyTeams`, etc.

Channel 0 = unreliable gameplay; channel 1 = reliable welcome/lobby.

## Rendering (Vulkan + GLFW)

| Path | Role |
|------|------|
| `game/include/zh/gfx/gfx.hpp` | Facade over the draw API |
| `game/include/zh/gfx/gfx_compat.hpp` | Raylib-like names (`DrawText`, `BeginMode3D`, …) |
| `game/src/gfx/vulkan/` | Vulkan device, swapchain, 2D/3D draw, fonts, textures |
| `game/resources/shaders/vk/` | GLSL sources → SPIR-V at build time (CMake) |
| `game/src/ui/map_shape_draw.cpp` | Map JSON → lit 3D playfield (ice, boards, shapes) |
| `game/src/ui/playing_scene_3d.cpp` | Playing-world draw stack |
| `game/src/ui/skater_fx.cpp` | Boost trail / slide spray particles |
| `game/src/platform/` | GLFW window + input, Win32 paths |

Playing uses a **3D camera** with sim coords on the ice plane (`zh/ui/world_space_3d.hpp`). Picking uses `playing_scene_pick.cpp`.

## Audio

| Path | Role |
|------|------|
| `game/include/zh/audio/audio_engine.hpp` | miniaudio wrapper |
| `game/src/audio/audio_engine.cpp` | Engine init, file load/play |
| `game/src/ui/game_sounds.cpp` | Gameplay MP3 cues |

## Build

| Script | OS |
|--------|-----|
| `game/scripts/build.bat` | Windows (CMake + VS + vcpkg) |
| `game/scripts/build.sh` | Linux (CMake + Ninja + vcpkg) |

Both write **`game/compile_commands.json`** for clangd. Local CMake output goes under **`game/build/`** (gitignored) — see `game/build/README.md`.

## Module rules (short)

Allowed dependency direction:

```
protocol  →  game  →  client / net  →  detail / ui  →  app facade
```

Lower layers must not `#include` UI or gfx draw headers. **`MatchWorld` is only mutated on the host** during the fixed sim step.

## Map editor (TypeScript)

| File | Role |
|------|------|
| `map-editor/src/main.ts` | Toolbar and sidebar UI |
| `map-editor/src/editor.ts` | Canvas tools, export |
| `map-editor/src/types.ts` | Map JSON types |

Details: [map-editor.md](map-editor.md).

## Typical tasks

| Goal | Start here |
|------|------------|
| New menu or HUD | `app_screen_draw.cpp`, `playing_hud.cpp` |
| Change 3D playfield / map draw | `map_shape_draw.cpp`, `playing_scene_3d.cpp` |
| Change skater move | `skater_physics.cpp`, `match_world.cpp` |
| Change puck / shots | `puck_physics.cpp` |
| New net field | `protocol.hpp` + host router + client session |
| New map field | `types.ts`, `map_io.cpp`, `map_runtime.cpp` |

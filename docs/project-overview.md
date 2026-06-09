# Project overview

ZH Standalone is a **4v4 hockey prototype**: server-authoritative sim on a 2D rink plane, **listen-server** multiplayer over **UDP (ENet)**, drawn with **Vulkan** (GLFW). The host runs the real simulation; remote clients send inputs and render predictions/interpolation.

## Repo layout

```
ZHStandalone/
├── game/           C++ game (CMake + vcpkg + Vulkan)
├── map-editor/     Browser map tool (Vite + TypeScript)
└── docs/           Developer guides (this folder)
```

### `game/`

| Path | Role |
|------|------|
| `src/` | C++ source |
| `include/zh/` | Public headers by module |
| `resources/` | Art, fonts, GLSL → SPIR-V shaders |
| `maps/` | Map JSON (`default.json`) |
| `cmake/` | CMake helpers (externals, vcpkg, shaders) |
| `scripts/` | `build.bat`, `build.sh`, `fetch_externals.*`, `run.*` |
| `vcpkg.json` | Manifest: glfw3, vulkan-loader, shaderc, woff2 |

### `map-editor/`

Edits the same map JSON the game loads. Dev server proxies `/resources/` to `../game/resources/` for previews.

## How a match works (short version)

1. **Host** starts a listen server and opens **Host Lobby**.
2. **Clients** join by IP/port, pick teams in lobby UI.
3. Host presses **Start** → fixed **60 Hz** sim on the host (`MatchWorld`).
4. Host sends **snapshots**; clients send **inputs** with sequence numbers.
5. Clients **predict** their own skater and **interpolate** everyone else.

Options screen can set **input delay B** (host fairness) and **client interp lag** (visual smoothing only).

## Simulation vs session

- **`zh::game::MatchWorld`** — puck, skaters, period, goals (host only).
- **`zh::detail::AppContext`** — screens, lobby, ENet peers, slot tables, render assets.

Keep new gameplay rules in `game/` (`MatchWorld`, physics). Keep connection/UI glue in `detail/` and `ui/`.

## Fixed timestep

Simulation runs at **60 Hz** (`zh::App::kFixedDt`). Rendering may run faster; the main loop can run multiple sim steps per frame (capped catch-up). Host draw extrapolates between sim ticks using `interp_alpha`.

See [code-tour.md](code-tour.md) for file-level pointers.

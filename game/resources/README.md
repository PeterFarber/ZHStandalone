# Runtime assets (`resources/`)

Files here are copied next to the game executable when you run **`game/scripts/build.bat`** or **`build.sh`**.

## Shaders (`resources/shaders/vk/`)

Vulkan GLSL sources compiled to `*.spv` beside the sources at build time (CMake + shaderc).

## UI fonts (`resources/fonts/`)

Vulkan `DrawText` loads from here (copied beside the exe at build time).

| File | Use |
|------|-----|
| `game.woff2` | Preferred UI font |
| `game.ttf` | TTF fallback if WOFF2 decode is unavailable |
| `ui.woff2` / `ui.ttf` | Alternate preferred names |
| Any other `.woff2` / `.ttf` / `.otf` | Used if no named file is found (first in folder) |

WOFF2 decoding uses the vcpkg `woff2` port. Windows fallback: Segoe UI / Arial if the folder is empty.

## Gameplay sounds (`resources/sounds/`)

MP3 cues loaded by `game/src/ui/game_sounds.cpp` (miniaudio). Expected names:

| File | Use |
|------|-----|
| `Zoom.mp3` | Z boost |
| `OneTimer.mp3` | C one-timer window start (local) |
| `Shield.mp3` | Goalie X shield start (local) |
| `Shot.mp3` | Charged slap release |
| `PuckCollision.mp3` | Loose puck hits boards |
| `PuckMetalCollision.mp3` | Loose puck hits goal frames |
| `Countdown.mp3` | Faceoff countdown start |

## Map backgrounds

Map JSON can reference image paths under `resources/` (e.g. repeating `background.png`). Export from the map editor or edit `game/maps/default.json` directly. The **3D playing rink** geometry comes from map shapes (`map_shape_draw.cpp`), not from a fixed sprite sheet.

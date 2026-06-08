# Runtime assets (`resources/`)

Files here are copied next to **`zh_game.exe`** when you run **`game\scripts\build.bat`**.

## Modular rink (`resources/rink/`)

| File | Use |
|------|-----|
| `ice_tile.png` | Tileable ice surface (128×128) |
| `board_horizontal.png` | Top/bottom straight boards |
| `board_vertical.png` | Side straight boards |
| `board_corner_tl/tr/bl/br.png` | Corner connectors |
| `board_end_left/right.png` | Rounded end-cap boards |
| `goal_left/right.png` | Goal mouth sprites (transparent top-down; see `tools/process_goal_sprite.py`) |

Regenerate placeholders: `python tools/generate_rink_assets.py`

## Player sprites (`resources/sprites/`)

| File | Use |
|------|-----|
| `player_skater.png` | Skater (Team A base; Team B hue-shifted at load) |
| `player_goalie.png` | Goalie (larger sprite in-game) |

Legacy fallback: `player_sprite.png` at repo root of `resources/` for skaters only.

## Gameplay sounds (`resources/sounds/`)

| File | Use |
|------|-----|
| `Zoom.mp3` | Z boost |
| `OneTimer.mp3` | C one-timer window start (local) |
| `Shield.mp3` | Goalie X shield start (local) |
| `Shot.mp3` | Charged slap release |
| `PuckCollision.mp3` | Loose puck hits boards |
| `PuckMetalCollision.mp3` | Loose puck hits goal frames |
| `Countdown.mp3` | Faceoff countdown start |

Replace any PNG with your own art — keep filenames and transparency.

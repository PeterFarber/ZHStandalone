# ZH Map Editor

Browser editor for rink maps used by **`game/`** in ZH Standalone.

## Run

```bat
cd map-editor
npm install
npm run dev
```

Open the URL Vite prints (usually `http://localhost:5173`).

Dev server proxies **`/resources/…`** to **`../game/resources/`** for background images and rink art previews.

## Workflow

1. Place visual pieces (ice, walls, circles, goals) with the canvas tools.
2. Add goal zones, blockers, and spawn points inside the green play area.
3. **Download JSON** and save as **`game/maps/default.json`**.
4. Rebuild: **`game\scripts\build.bat`** (maps copy to **`game/bin/Release/maps/`**).

## Play area

Use **Map settings** in the sidebar to change play area width/height (expands around center). **`player_bounds`** and **`camera_bounds`** are written on export.

## Objects panel

The **Objects** sidebar lists placed shapes. Select an entry to focus it on the canvas. **Lock** an object to keep it selectable but prevent accidental moves.

## Map format

Shared with the game: **`game/maps/default.json`**, **`game/include/zh/game/map_definition.hpp`**, schema version `1`.

## Rink sprite previews

Optional copies under **`map-editor/public/rink/`** for the tile picker. When game art changes, sync from the game:

```bat
xcopy /Y ..\game\resources\rink\*.png public\rink\
```

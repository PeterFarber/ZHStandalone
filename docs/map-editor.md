# Map editor (for developers)

The editor produces **`game/maps/default.json`**, which the game loads at startup.

## Run locally

```bat
cd map-editor
npm install
npm run dev
```

Node 20+. Vite serves the app and proxies **`/resources/`** to **`../game/resources/`** so rink PNGs match the game.

## Workflow

1. Place tiles and colliders on the canvas.
2. Set spawn points, goals, and blockers inside the play area.
3. Use **Map settings** to resize the play area if needed.
4. **Download JSON** → save as **`game/maps/default.json`**.
5. Rebuild the game so `maps/` is copied beside the exe.

User-facing notes: [map-editor/README.md](../map-editor/README.md).

## Code layout

| File | Purpose |
|------|---------|
| `src/editor.ts` | `MapEditor` class — tools, selection, history, export |
| `src/main.ts` | DOM wiring (toolbar, sidebars) |
| `src/types.ts` | JSON shape (keep in sync with C++) |
| `src/normalizeMap.ts` | Export cleanup (bounds, schema version) |
| `src/mapFrame.ts` | Play area width/height |
| `src/objectList.ts` / `objectLocks.ts` | Objects panel |

C++ counterpart: **`game/include/zh/game/map_definition.hpp`**, loader in **`game/src/game/map_io.cpp`**.

## Schema changes

When you add a JSON field:

1. `map-editor/src/types.ts` + export path.
2. C++ struct + parse in `map_io.cpp`.
3. Use it in `map_runtime.cpp` or sim if gameplay needs it.

Bump **`schema_version`** in export if you break old files.

## Preview tiles

Optional copies under **`map-editor/public/rink/`**. Refresh from game art when PNGs change:

```bat
xcopy /Y ..\game\resources\rink\*.png public\rink\
```

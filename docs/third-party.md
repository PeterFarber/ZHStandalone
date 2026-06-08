# Third-party software

Game code is MIT (see [LICENSE](../LICENSE)). Bundled or fetched dependencies:

## Game (C++)

| Component | License | How it arrives |
|-----------|---------|----------------|
| [raylib](https://www.raylib.com/) | zlib/libpng | Premake download → `game/build/external/` |
| [ENet](http://enet.bespin.org/) | MIT | Premake download → `game/build/external/` |
| [nlohmann/json](https://github.com/nlohmann/json) | MIT | Premake download → `game/build/external/` |
| GLFW | zlib/libpng | Built as part of raylib |

## Map editor (npm)

| Package | License |
|---------|---------|
| Vite | MIT |
| TypeScript | Apache-2.0 |
| Tailwind CSS | MIT |

Full npm tree: `map-editor/package-lock.json`.

## Art

PNG files under **`game/resources/`** are project assets. Confirm you have rights to redistribute before publishing the repo.

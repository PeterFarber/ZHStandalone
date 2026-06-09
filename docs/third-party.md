# Third-party software

Game code is MIT (see [LICENSE](../LICENSE)). Bundled or fetched dependencies:

## Game (C++)

| Component | License | How it arrives |
|-----------|---------|----------------|
| [Vulkan loader / headers](https://www.vulkan.org/) | Apache-2.0 / MIT (Khronos) | vcpkg manifest (`vcpkg.json`) |
| [GLFW](https://www.glfw.org/) | zlib/libpng | vcpkg manifest |
| [shaderc / glslc](https://github.com/google/shaderc) | Apache-2.0 | vcpkg manifest (SPIR-V compile at build time) |
| [woff2](https://github.com/google/woff2) | MIT | vcpkg manifest (optional UI font decode) |
| [ENet](http://enet.bespin.org/) | MIT | `scripts/fetch_externals.*` → `game/build/external/` |
| [nlohmann/json](https://github.com/nlohmann/json) | MIT | fetch script |
| [miniaudio](https://github.com/mackron/miniaudio) | Public domain / MIT | fetch script (single header) |
| [stb](https://github.com/nothings/stb) | Public domain / MIT | fetch script (`stb_truetype.h` for UI fonts) |
| [glm](https://github.com/g-truc/glm) | MIT | fetch script |
| [volk](https://github.com/zeux/volk) | MIT | fetch script |

Third-party sources under `game/build/external/` are fetched by `scripts/fetch_externals.*` on first build.

## Map editor (npm)

| Package | License |
|---------|---------|
| Vite | MIT |
| TypeScript | Apache-2.0 |
| Tailwind CSS | MIT |

Full npm tree: `map-editor/package-lock.json`.

## Art

PNG/WOFF2 files under **`game/resources/`** are project assets. Confirm you have rights to redistribute before publishing the repo.

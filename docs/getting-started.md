# Getting started

## Prerequisites

**Windows:** Visual Studio 2022 (or Build Tools) with C++ workload, **Git**, and **CMake** (bundled with VS). First build bootstraps [vcpkg](https://github.com/microsoft/vcpkg) under `%USERPROFILE%\vcpkg` unless `VCPKG_ROOT` is already set.

**Linux:** `build-essential`, `cmake`, `ninja`, `curl`, `unzip`, `git`, X11 dev packages, and **Vulkan** dev packages (`libvulkan-dev`, `mesa-vulkan-drivers` or vendor drivers).

**Map editor:** Node.js 20+.

## Build the game

Windows (from repo root):

```bat
game\scripts\build.bat
game\scripts\run.bat
```

Linux:

```bash
sudo apt install build-essential cmake ninja-build curl unzip git pkg-config \
  libvulkan-dev mesa-vulkan-drivers libx11-dev libxrandr-dev libxi-dev \
  libxcursor-dev libxinerama-dev
chmod +x game/scripts/*.sh
game/scripts/build.sh
game/scripts/run.sh
```

The executable lands in `game/bin/Release/` (`zh_game.exe` on Windows, `zh_game` on Linux). The build copies `game/resources/` and `game/maps/` next to it. Windows builds also place required DLLs (`vulkan-1.dll`, `glfw3.dll`, etc.) in that folder.

The first build downloads **vcpkg ports** (GLFW, Vulkan loader, shaderc, woff2) and fetches **ENet**, **nlohmann/json**, **miniaudio**, **stb**, **glm**, and **volk** into `game/build/external/` — network access required once.

Each build writes **`game/compile_commands.json`** for clangd (gitignored). On Windows this uses a separate Ninja tree under `build/cmake-clangd/`; on Linux it comes from the main `build/cmake-vk/` configure. To refresh without rebuilding:

```bat
game\scripts\gen_compile_commands.bat
```

```bash
game/scripts/gen_compile_commands.sh
```

## Map editor

```bat
cd map-editor
npm install
npm run dev
```

Open the URL Vite prints (usually `http://localhost:5173`). Export JSON to `game/maps/default.json`, then run `game\scripts\build.bat` again.

## Settings files

| OS | Path |
|----|------|
| Windows | `%LOCALAPPDATA%\ZHStandalone\settings.json` |
| Linux | `~/.config/zh-standalone/settings.json` |

## Common issues

| Problem | Fix |
|---------|-----|
| Linker cannot open `zh_game.exe` | Close the running game, rebuild |
| Missing art or maps at runtime | Re-run `build.bat` / `build.sh` (copies `resources/` and `maps/`) |
| Home screen clicks wrong on Windows | Already handled in code (DPI flags); report if it persists |
| `Missing enet` / external headers | Run `game/scripts/fetch_externals.ps1` or `fetch_externals.sh` |
| SPIR-V / shader errors | Ensure vcpkg installed `shaderc`; rebuild so `*.spv` are generated |
| `gen_compile_commands` / Ninja fails on Windows | Requires VS 2022 C++ workload; `build.bat` runs `vcvars64` automatically |

## Visual Studio (optional)

Open the CMake project directly (`game/CMakeLists.txt`) or generate under `game/build/cmake-vk/` via `build.bat`.

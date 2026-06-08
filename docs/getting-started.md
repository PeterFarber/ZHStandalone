# Getting started

## Prerequisites

**Windows:** MinGW with `mingw32-make` and `g++` on PATH. [w64devkit](https://github.com/skeeto/w64devkit) works well at `%USERPROFILE%\Documents\CPP\w64devkit\` — the build script picks that up automatically.

**Linux:** `build-essential`, `premake5`, and OpenGL/X11 dev packages (see commands below).

**Map editor:** Node.js 20+.

## Build the game

Windows (from repo root):

```bat
game\scripts\build.bat
game\scripts\run.bat
```

Linux:

```bash
sudo apt install build-essential premake5 libgl1-mesa-dev libx11-dev \
  libxrandr-dev libxi-dev libxcursor-dev libxinerama-dev
chmod +x game/scripts/*.sh
game/scripts/build.sh
game/scripts/run.sh
```

The executable lands in `game/bin/Release/` (`zh_game.exe` on Windows, `zh_game` on Linux). The build copies `game/resources/` and `game/maps/` next to it.

The first build downloads **raylib**, **ENet**, and **nlohmann/json** via Premake — you need network access once.

## IntelliSense (optional)

After a successful build:

```bat
game\scripts\gen_compile_commands.bat
```

That writes `game/compile_commands.json` for clangd / VS Code (gitignored).

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
| Missing art or maps at runtime | Re-run `build.bat` / `build.sh` |
| Home screen clicks wrong on Windows | Already handled in code (DPI flags); report if it persists |

## Visual Studio (optional)

```bat
game\scripts\build-VisualStudio2022.bat
```

Open the `.sln` under `game/build/`.

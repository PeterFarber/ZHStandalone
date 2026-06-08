# Game scripts

## Windows

From repo root:

```bat
game\scripts\build.bat
game\scripts\run.bat
game\scripts\gen_compile_commands.bat
```

| Script | Purpose |
|--------|---------|
| **`build.bat`** | Premake + MinGW release build; copies `resources/` and `maps/` into `bin/Release/` |
| **`run.bat`** | Launch `bin/Release/zh_game.exe` |
| **`gen_compile_commands.bat`** | Regenerate `game/compile_commands.json` for clangd |
| **`build-VisualStudio2022.bat`** | Generate VS solution under `build/` |

## Linux / macOS

```bash
chmod +x game/scripts/*.sh   # once after clone
game/scripts/build.sh
game/scripts/run.sh
game/scripts/gen_compile_commands.sh
```

Requires **`premake5`**, **`make`**, and OpenGL/X11 dev packages (see **`docs/getting-started.md`**).

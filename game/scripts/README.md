# Game scripts

## Windows (primary)

From repo root:

```bat
game\scripts\build.bat
game\scripts\run.bat
```

| Script | Purpose |
|--------|---------|
| **`build.bat`** | CMake + vcpkg release build; honors `VCPKG_ROOT` if set (CI uses repo-local vcpkg); writes `compile_commands.json` |
| **`fetch_externals.ps1`** | Download ENet, json, miniaudio, stb, glm, volk into `build/external/` |
| **`setup_vcpkg.ps1`** | Bootstrap `%USERPROFILE%\vcpkg` when `VCPKG_ROOT` unset |
| **`compile_shaders.bat`** | Optional manual SPIR-V compile (CMake builds shaders automatically) |
| **`run.bat`** | Launch `bin/Release/zh_game.exe` |
| **`gen_compile_commands.bat`** | Refresh `compile_commands.json` only (Ninja configure; also run automatically by **`build.bat`**) |

## Linux

```bash
chmod +x game/scripts/*.sh   # once after clone
game/scripts/build.sh
game/scripts/run.sh
```

| Script | Purpose |
|--------|---------|
| **`build.sh`** | CMake + Ninja + vcpkg release build; writes `compile_commands.json` |
| **`fetch_externals.sh`** | Same third-party fetch as the PowerShell script |
| **`gen_compile_commands.sh`** | Refresh `compile_commands.json` only (also run automatically by **`build.sh`**) |

Requires packages listed in **`docs/getting-started.md`**.

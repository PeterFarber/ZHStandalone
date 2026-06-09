# Build working directory

This folder is **not** source code. CMake and fetch scripts write here locally (gitignored):

| Path | Created by |
|------|------------|
| `external/` | `scripts/fetch_externals.ps1` or `fetch_externals.sh` |
| `cmake-vk/` | `scripts/build.bat` / `build.sh` |
| `cmake-clangd/` | Windows IntelliSense-only Ninja configure (`gen_compile_commands.bat`; also run at end of `build.bat`) |
| `.cache/` | clangd index cache |

After clone, run `game/scripts/build.bat` (Windows) or `build.sh` (Linux). Nothing under this folder needs to be committed.

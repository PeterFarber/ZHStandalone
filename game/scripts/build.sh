#!/usr/bin/env bash
set -euo pipefail

GAME="$(cd "$(dirname "$0")/.." && pwd)"
cd "$GAME/build"

if command -v premake5 >/dev/null 2>&1; then
  premake5 gmake
elif [[ -x ./premake5 ]]; then
  ./premake5 gmake
else
  echo "[build.sh] error: premake5 not found (install premake5 or add to PATH)" >&2
  exit 1
fi

cd "$GAME"
make config=release_x64 -j"$(nproc)"

copy_tree() {
  local src="$1"
  local dst="$2"
  if [[ -d "$src" ]]; then
    mkdir -p "$dst"
    cp -a "$src/." "$dst/"
  fi
}

if [[ -f "$GAME/bin/Release/zh_game" || -f "$GAME/bin/Release/zh_game.exe" ]]; then
  copy_tree "$GAME/resources" "$GAME/bin/Release/resources"
  copy_tree "$GAME/maps" "$GAME/bin/Release/maps"
fi
if [[ -f "$GAME/bin/Debug/zh_game" || -f "$GAME/bin/Debug/zh_game.exe" ]]; then
  copy_tree "$GAME/resources" "$GAME/bin/Debug/resources"
  copy_tree "$GAME/maps" "$GAME/bin/Debug/maps"
fi

echo "[build.sh] ok: $GAME/bin/Release/zh_game"

#!/usr/bin/env bash
# CMake + vcpkg manifest (game/vcpkg.json).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

VCPKG_ROOT="${VCPKG_ROOT:-}"
if [[ -n "$VCPKG_ROOT" && ! -x "$VCPKG_ROOT/vcpkg" ]]; then
  VCPKG_ROOT=""
fi
VCPKG_ROOT="${VCPKG_ROOT:-$HOME/vcpkg}"
if [[ ! -x "$VCPKG_ROOT/vcpkg" ]]; then
  if [[ ! -d "$VCPKG_ROOT" ]]; then
    echo "[build.sh] cloning vcpkg to $VCPKG_ROOT"
    git clone https://github.com/microsoft/vcpkg.git "$VCPKG_ROOT"
  fi
  "$VCPKG_ROOT/bootstrap-vcpkg.sh" -disableMetrics
fi

TRIPLET="${VCPKG_TARGET_TRIPLET:-x64-linux}"
TOOLCHAIN="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"

echo "[build.sh] VCPKG_ROOT=$VCPKG_ROOT triplet=$TRIPLET"
"$VCPKG_ROOT/vcpkg" install --x-manifest-root="$ROOT" --triplet "$TRIPLET"

if [[ ! -f build/external/enet/include/enet/enet.h ]]; then
  bash scripts/fetch_externals.sh
fi

BUILD_DIR=build/cmake-vk
cmake -S . -B "$BUILD_DIR" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
  -DVCPKG_TARGET_TRIPLET="$TRIPLET"
cp -f "$BUILD_DIR/compile_commands.json" "$ROOT/compile_commands.json"
echo "[build.sh] compile_commands: $ROOT/compile_commands.json"
cmake --build "$BUILD_DIR" --parallel

if [[ ! -x bin/Release/zh_game ]]; then
  echo "[build.sh] error: bin/Release/zh_game not found" >&2
  exit 1
fi

echo "[build.sh] ok: $ROOT/bin/Release/zh_game"

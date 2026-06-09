#!/usr/bin/env bash
# Refresh compile_commands.json via Ninja configure in build/cmake-vk/.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

VCPKG_ROOT="${VCPKG_ROOT:-}"
if [[ -n "$VCPKG_ROOT" && ! -x "$VCPKG_ROOT/vcpkg" ]]; then
  VCPKG_ROOT=""
fi
VCPKG_ROOT="${VCPKG_ROOT:-$HOME/vcpkg}"
if [[ ! -x "$VCPKG_ROOT/vcpkg" ]]; then
  echo "[gen_compile_commands.sh] error: vcpkg not found at $VCPKG_ROOT" >&2
  exit 1
fi

TRIPLET="${VCPKG_TARGET_TRIPLET:-x64-linux}"
TOOLCHAIN="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"

if [[ ! -f build/external/enet/include/enet/enet.h ]]; then
  bash scripts/fetch_externals.sh
fi

BUILD_DIR=build/cmake-vk
cmake -S . -B "$BUILD_DIR" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
  -DVCPKG_TARGET_TRIPLET="$TRIPLET"

cp -f "$BUILD_DIR/compile_commands.json" "$ROOT/compile_commands.json"
echo "[gen_compile_commands.sh] ok: $ROOT/compile_commands.json"

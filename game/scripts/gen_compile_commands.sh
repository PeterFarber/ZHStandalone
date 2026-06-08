#!/usr/bin/env bash
set -euo pipefail

GAME="$(cd "$(dirname "$0")/.." && pwd)"
cd "$GAME/build"

PREMAKE=""
if command -v premake5 >/dev/null 2>&1; then
  PREMAKE=premake5
elif [[ -x ./premake5 ]]; then
  PREMAKE=./premake5
else
  echo "[gen_compile_commands.sh] error: premake5 not found" >&2
  exit 1
fi

"$PREMAKE" ecc
cp -f "$GAME/build/compile_commands.json" "$GAME/compile_commands.json"
echo "[gen_compile_commands.sh] ok: $GAME/compile_commands.json"

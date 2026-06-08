#!/usr/bin/env bash
set -euo pipefail

GAME="$(cd "$(dirname "$0")/.." && pwd)"

EXE=""
if [[ -x "$GAME/bin/Release/zh_game" ]]; then
  EXE="$GAME/bin/Release/zh_game"
elif [[ -f "$GAME/bin/Release/zh_game.exe" ]]; then
  EXE="$GAME/bin/Release/zh_game.exe"
elif [[ -x "$GAME/bin/Debug/zh_game" ]]; then
  EXE="$GAME/bin/Debug/zh_game"
elif [[ -f "$GAME/bin/Debug/zh_game.exe" ]]; then
  EXE="$GAME/bin/Debug/zh_game.exe"
fi

if [[ -z "$EXE" ]]; then
  echo "[run.sh] zh_game not found under bin/Release or bin/Debug" >&2
  echo "[run.sh] run game/scripts/build.sh first" >&2
  exit 1
fi

cd "$(dirname "$EXE")"
exec "$EXE" "$@"

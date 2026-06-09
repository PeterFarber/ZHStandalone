#!/usr/bin/env bash
# Download vendored third-party sources into build/external (CMake build).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
EXT="$ROOT/build/external"
mkdir -p "$EXT"

download_file() {
  local url="$1"
  local dest="$2"
  if [[ -f "$dest" ]]; then
    return 0
  fi
  echo "[fetch] $url"
  curl -fsSL "$url" -o "$dest"
}

ensure_zip() {
  local url="$1"
  local zip_name="$2"
  local folder_name="$3"
  local rename_to="$4"
  local dir="$EXT/$rename_to"
  if [[ -d "$dir" ]]; then
    return 0
  fi
  local zip="$EXT/$zip_name"
  download_file "$url" "$zip"
  echo "[fetch] unzip $zip_name -> $rename_to"
  unzip -qo "$zip" -d "$EXT"
  if [[ "$rename_to" != "$folder_name" && -d "$EXT/$folder_name" ]]; then
    mv "$EXT/$folder_name" "$dir"
  fi
  rm -f "$zip"
}

ensure_zip \
  "https://github.com/lsalzman/enet/archive/refs/heads/master.zip" \
  "enet-master.zip" "enet-master" "enet"

ensure_zip \
  "https://github.com/nlohmann/json/archive/refs/tags/v3.11.3.zip" \
  "json.zip" "json-3.11.3" "nlohmann"

ma_dir="$EXT/miniaudio"
mkdir -p "$ma_dir"
download_file \
  "https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h" \
  "$ma_dir/miniaudio.h"

stb_dir="$EXT/stb"
mkdir -p "$stb_dir"
download_file "https://raw.githubusercontent.com/nothings/stb/master/stb_truetype.h" "$stb_dir/stb_truetype.h"

ensure_zip \
  "https://github.com/g-truc/glm/archive/refs/tags/1.0.1.zip" \
  "glm.zip" "glm-1.0.1" "glm"

ensure_zip \
  "https://github.com/zeux/volk/archive/refs/heads/master.zip" \
  "volk.zip" "volk-master" "volk"

echo "[fetch] done: $EXT"

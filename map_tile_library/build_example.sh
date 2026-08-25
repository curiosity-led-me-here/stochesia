#!/bin/zsh
set -euo pipefail

root="$(cd "$(dirname "$0")" && pwd)"
core_root="$(cd "$root/.." && pwd)"
cd "$root"
mkdir -p "$core_root/bin/map_tile_library"
clang++ -std=c++17 -Wall -Wextra \
  -I"$root/include" \
  "$root/examples/layered_example.cpp" \
  "$root/src/map_tile_library.mm" \
  -framework CoreFoundation \
  -framework CoreGraphics \
  -framework ImageIO \
  -o "$core_root/bin/map_tile_library/layered_example"

"$core_root/bin/map_tile_library/layered_example" "$root"

#!/bin/zsh
set -euo pipefail

root="$(cd "$(dirname "$0")" && pwd)"
cd "$root"
clang++ -std=c++17 -Wall -Wextra \
  -I"$root/include" \
  "$root/examples/layered_example.cpp" \
  "$root/src/map_tile_library.mm" \
  -framework CoreFoundation \
  -framework CoreGraphics \
  -framework ImageIO \
  -o "$root/layered_example"

"$root/layered_example" "$root"

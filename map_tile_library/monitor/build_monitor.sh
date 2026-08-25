#!/bin/zsh
set -euo pipefail

library_root="$(cd "$(dirname "$0")/.." && pwd)"
core_root="$(cd "$library_root/.." && pwd)"
mkdir -p "$core_root/bin/map_tile_library/monitor"

clang++ -std=c++17 -fobjc-arc -Wall -Wextra \
  -Wno-sign-compare -Wno-logical-op-parentheses -Wno-unused-variable \
  -Wno-unused-parameter -Wno-missing-field-initializers \
  -I"$library_root/include" \
  -I"$core_root/include" \
  -I"$core_root/src" \
  "$library_root/monitor/main.cpp" \
  "$library_root/src/map_monitor.mm" \
  "$library_root/src/map_tile_library.mm" \
  "$library_root/src/fe8_unit_visuals.cpp" \
  "$library_root/src/fe8_unit_data.cpp" \
  "$library_root/src/occupancy_overlay.cpp" \
  "$library_root/src/logic_render_control.cpp" \
  "$library_root/src/entity_animation.cpp" \
  "$core_root/src/maps.cpp" \
  "$core_root/src/game_data.cpp" \
  "$core_root/src/terrain_data.cpp" \
  "$core_root/src/entity_data.cpp" \
  "$core_root/src/entity_registry.cpp" \
  "$core_root/src/general_pathtracing.cpp" \
  "$core_root/src/pathfinder.cpp" \
  "$core_root/src/mechanics.cpp" \
  "$core_root/src/map_ascii.cpp" \
  "$core_root/src/mechanics_ascii.cpp" \
  -framework Cocoa \
  -framework CoreFoundation \
  -framework CoreGraphics \
  -framework ImageIO \
  -o "$core_root/bin/map_tile_library/monitor/fe8_tactical_monitor"

echo "Built: $core_root/bin/map_tile_library/monitor/fe8_tactical_monitor"

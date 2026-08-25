#!/bin/zsh
set -euo pipefail

viewer_root="$(cd "$(dirname "$0")" && pwd)"
core_root="$(cd "$viewer_root/../.." && pwd)"
"$viewer_root/build_workbench.sh"
exec "$core_root/bin/map_tile_library/viewer/fe8_map_workbench" "$@"

#!/bin/zsh
set -euo pipefail

viewer_root="$(cd "$(dirname "$0")" && pwd)"
"$viewer_root/build_workbench.sh"
exec "$viewer_root/build/fe8_map_workbench" "$@"

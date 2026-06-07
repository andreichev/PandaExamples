#!/usr/bin/env sh
set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)"
PRESET="${1:-macos-debug}"

cd "$ROOT/Export"
cmake --preset "$PRESET"

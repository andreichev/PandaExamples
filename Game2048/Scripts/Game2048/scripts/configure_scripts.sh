#!/usr/bin/env sh
set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)"
PRESET="${1:-debug}"

cd "$ROOT/Scripts/Game2048"
cmake --preset "$PRESET"

#!/usr/bin/env sh
set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)"
PRESET="${1:-build-debug}"

cd "$ROOT/Scripts/HelloUI"
cmake --build --preset "$PRESET"

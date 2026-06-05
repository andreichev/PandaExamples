#!/usr/bin/env sh
set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)"
PRESET="${1:-ios-device-debug}"
PROJECT_DIR="$ROOT/Export/build/$PRESET"

cd "$ROOT/Export"
cmake --preset "$PRESET"

PROJECT_PATH=""
if [ -d "$PROJECT_DIR" ]; then
    PROJECT_PATH="$(find "$PROJECT_DIR" -maxdepth 1 -name '*.xcodeproj' -print -quit)"
fi
if [ -z "$PROJECT_PATH" ]; then
    echo "Xcode project not found in: $PROJECT_DIR"
    exit 1
fi

open "$PROJECT_PATH"

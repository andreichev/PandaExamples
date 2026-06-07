#!/usr/bin/env sh
set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)"

cd "$ROOT/Scripts/Game2048"
./format.sh

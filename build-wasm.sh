#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
if ! command -v em++ >/dev/null 2>&1; then
  echo "Emscripten not found. Install and activate the Emscripten SDK, then run this script." >&2
  exit 1
fi
em++ tinybasic.cpp -std=c++17 -O2 -o tinybasic.js \
  -s WASM=1 -s MODULARIZE=1 -s EXPORT_NAME=createTinyBasicModule \
  -s ENVIRONMENT=web -s ALLOW_MEMORY_GROWTH=1 \
  -s EXPORTED_FUNCTIONS='["_runTinyBasic"]' \
  -s EXPORTED_RUNTIME_METHODS='["ccall"]'
echo "Built tinybasic.js and tinybasic.wasm"

#!/usr/bin/env bash
set -euo pipefail

cmake -S server -B build
cmake --build build

./build/cube_server "${1:-}"

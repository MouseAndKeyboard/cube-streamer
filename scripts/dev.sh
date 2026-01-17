#!/usr/bin/env bash
set -euo pipefail

cmake -S server -B build
cmake --build build

( cd client && npm install && npm run dev )

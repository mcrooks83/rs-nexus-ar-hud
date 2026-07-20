#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "$0")/env.sh"

cd "$ZEPHYR_WORKSPACE"

west blobs fetch hal_espressif

west build -p always \
  -b xiao_esp32s3/esp32s3/procpu/sense \
  "$APP" \
  --build-dir "$APP/build"